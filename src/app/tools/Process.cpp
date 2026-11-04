// 进程与压缩包：启动子进程、等待、解压。
#include "app/tools/tools.h"

#include <chrono>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tools {

bool RunAndWait(const std::vector<std::string>& args, std::string& error) {
    return RunAndWait(args, std::string(), error);
}

bool RunAndWait(const std::vector<std::string>& args, const std::string& log_path, std::string& error) {
#ifdef _WIN32
    std::string command_line;
    for (const auto& arg : args) {
        if (!command_line.empty()) command_line.push_back(' ');
        bool quoted = arg.find(' ') != std::string::npos;
        if (quoted) command_line.push_back('"');
        command_line += arg;
        if (quoted) command_line.push_back('"');
    }

    HANDLE log_file = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES attributes{};
    if (!log_path.empty()) {
        attributes.nLength = sizeof(attributes);
        attributes.bInheritHandle = TRUE;
        log_file = CreateFileA(log_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &attributes,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (log_file == INVALID_HANDLE_VALUE) {
            error = "无法创建进程日志: " + log_path;
            return false;
        }
    }

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    if (log_file != INVALID_HANDLE_VALUE) {
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = log_file;
        startup.hStdError = log_file;
    }

    PROCESS_INFORMATION info{};
    std::vector<char> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back('\0');

    const BOOL inherit = log_file != INVALID_HANDLE_VALUE ? TRUE : FALSE;
    if (!CreateProcessA(nullptr, mutable_command.data(), nullptr, nullptr, inherit, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startup, &info)) {
        if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
        error = "CreateProcess 失败 " + std::to_string(GetLastError());
        return false;
    }
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);

    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    if (code != 0) {
        error = "退出码 " + std::to_string(code);
        return false;
    }
    return true;
#else
    (void)args;
    (void)log_path;
    error = "platform_not_supported";
    return false;
#endif
}

void SleepMs(int milliseconds) {
#ifdef _WIN32
    Sleep(static_cast<DWORD>(milliseconds));
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif
}

bool ExtractArchive(const std::string& archive, const std::string& dest, std::string& error) {
    if (!PathExists(archive)) {
        error = "压缩包不存在: " + archive;
        return false;
    }
    std::string dir_error;
    if (!EnsureDir(dest, dir_error)) {
        error = dir_error;
        return false;
    }
    return RunAndWait({"tar", "-xf", archive, "-C", dest}, error);
}

bool ExtractEntry(const std::string& archive, const std::string& entry, const std::string& dest,
                  std::string& out_path, std::string& error) {
    if (!PathExists(archive)) {
        error = "压缩包不存在: " + archive;
        return false;
    }
    std::string dir_error;
    if (!EnsureDir(dest, dir_error)) {
        error = dir_error;
        return false;
    }

    out_path = JoinPath(dest, entry);
    if (PathExists(out_path)) return true;

    std::string run_error;
    if (!RunAndWait({"tar", "-xf", archive, "-C", dest, entry}, run_error) ||
        !PathExists(out_path)) {
        error = "安装器里没有条目: " + entry;
        return false;
    }
    return true;
}

}  // namespace tools
