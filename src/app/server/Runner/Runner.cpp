#include "app/server/Runner/Runner.h"

#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace server {

namespace {

HANDLE ToHandle(void* handle) { return static_cast<HANDLE>(handle); }

std::wstring Widen(const std::string& text) {
    if (text.empty()) return {};
    const int size =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

// 路径/参数统一加引号：带空格的 java、jar、-cp 路径不加引号会被 CreateProcess 拆开
std::wstring Quote(const std::wstring& text) { return L"\"" + text + L"\""; }

// 只给含空格（或空）的参数加引号，-Xmx 这类简单参数保持原样
bool NeedsQuote(const std::string& arg) {
    return arg.empty() || arg.find_first_of(" \t") != std::string::npos;
}

std::string LastError() { return std::to_string(static_cast<int>(GetLastError())); }

}  // namespace

Runner::~Runner() { CloseHandles(); }

bool Runner::Start(const std::string& java, const std::vector<std::string>& args,
                   const std::string& work_dir, const std::string& log_path, std::string& error) {
    if (process_ != nullptr) {
        if (IsRunning()) {
            error = "服务端已在运行（pid " + std::to_string(pid_) + "）";
            return false;
        }
        CloseHandles();
    }
    pid_ = 0;
    exit_code_ = -1;

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    HANDLE stdin_read = nullptr;
    HANDLE stdin_write = nullptr;
    if (!CreatePipe(&stdin_read, &stdin_write, &attributes, 0)) {
        error = "创建服务端 stdin 管道失败（错误码 " + LastError() + "）";
        return false;
    }
    // 写端只给父进程用，别让子进程也继承一份，否则父进程关闭后子进程的 stdin 不会收到 EOF
    if (!SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0)) {
        error = "设置 stdin 管道句柄失败（错误码 " + LastError() + "）";
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        return false;
    }

    // stdout/stderr 都指向同一个日志文件，服务端输出按 UTF-8 原样落盘
    HANDLE log_file = CreateFileW(Widen(log_path).c_str(), GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log_file == INVALID_HANDLE_VALUE) {
        error = "无法创建服务端日志文件: " + log_path + "（错误码 " + LastError() + "）";
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        return false;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = stdin_read;
    startup.hStdOutput = log_file;
    startup.hStdError = log_file;

    // CreateProcessW 可能就地修改命令行缓冲，不能传 const 字符串的 c_str()
    std::wstring command = Quote(Widen(java));
    for (const auto& arg : args) {
        const std::wstring wide = Widen(arg);
        command += L" ";
        command += NeedsQuote(arg) ? Quote(wide) : wide;
    }
    const std::wstring work = Widen(work_dir);

    PROCESS_INFORMATION info{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, work.empty() ? nullptr : work.c_str(), &startup,
                        &info)) {
        const std::string code = LastError();
        error = "启动服务端失败（Java: " + java + "，工作目录: " + work_dir + "，错误码 " + code +
                "）";
        CloseHandle(log_file);
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        return false;
    }

    // 子进程已经拿到自己的副本，父进程这边多余的句柄要关掉
    CloseHandle(log_file);
    CloseHandle(stdin_read);
    CloseHandle(info.hThread);

    process_ = info.hProcess;
    stdin_write_ = stdin_write;
    pid_ = static_cast<int>(info.dwProcessId);
    exit_code_ = -1;
    return true;
}

void Runner::CloseHandles() {
    if (stdin_write_ != nullptr) {
        CloseHandle(ToHandle(stdin_write_));
        stdin_write_ = nullptr;
    }
    if (process_ != nullptr) {
        CloseHandle(ToHandle(process_));
        process_ = nullptr;
    }
}

bool Runner::IsRunning() const {
    if (process_ == nullptr) return false;
    return WaitForSingleObject(ToHandle(process_), 0) == WAIT_TIMEOUT;
}

int Runner::ExitCode() const {
    if (process_ == nullptr) return exit_code_;
    DWORD code = 0;
    if (!GetExitCodeProcess(ToHandle(process_), &code)) return exit_code_;
    if (code == STILL_ACTIVE) return -1;
    return static_cast<int>(code);
}

bool Runner::Send(const std::string& line, std::string& error) {
    if (stdin_write_ == nullptr || process_ == nullptr) {
        error = "服务器没有运行";
        return false;
    }
    if (!IsRunning()) {
        error = "服务器已经退出，无法发送命令（pid " + std::to_string(pid_) + "）";
        return false;
    }

    const std::string data = line + "\r\n";
    size_t sent = 0;
    while (sent < data.size()) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>(data.size() - sent);
        if (!WriteFile(ToHandle(stdin_write_), data.data() + sent, chunk, &written, nullptr) ||
            written == 0) {
            error = "发送服务端命令失败（pid " + std::to_string(pid_) + "，错误码 " + LastError() +
                    "）";
            return false;
        }
        sent += written;
    }
    return true;
}

bool Runner::Stop(std::string& error) { return Send("stop", error); }

bool Runner::ForceStop(std::string& error) {
    if (process_ == nullptr) {
        error = "服务器没有运行";
        return false;
    }
    if (!TerminateProcess(ToHandle(process_), 1)) {
        error = "强制结束服务端进程失败（pid " + std::to_string(pid_) + "，错误码 " + LastError() +
                "）";
        return false;
    }
    exit_code_ = 1;
    return true;
}

void Runner::Detach() {
    // 只放掉句柄，进程继续独立运行
    CloseHandles();
}

bool Runner::PidAlive(int pid) {
    if (pid <= 0) return false;
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 static_cast<DWORD>(pid));
    if (process == nullptr) return false;
    const DWORD wait = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return wait == WAIT_TIMEOUT;
}

bool Runner::KillPid(int pid, std::string& error) {
    if (pid <= 0) {
        error = "进程号无效: " + std::to_string(pid);
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr) {
        error = "无法打开服务端进程（pid " + std::to_string(pid) + "，错误码 " + LastError() + "）";
        return false;
    }
    const BOOL terminated = TerminateProcess(process, 1);
    const std::string code = LastError();
    CloseHandle(process);
    if (!terminated) {
        error = "强制结束服务端进程失败（pid " + std::to_string(pid) + "，错误码 " + code + "）";
        return false;
    }
    return true;
}

}  // namespace server
