#include "app/directory/Directory.h"
#include "app/tools/tools.h"

#include <cctype>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <thread>
#include <fstream>
#include <iterator>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>

#include <vector>
#endif

namespace fs = std::filesystem;

namespace {

std::string ToUtf8(const fs::path& path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

// 临时响应文件必须唯一：多个线程/实例同时 HttpGet 时固定文件名会互相覆盖。
// 放 RML 数据目录而不是 %TEMP%，避免受限环境里子进程不能写系统临时目录。
std::string TempBodyPath() {
    static std::atomic<unsigned> counter{0};
    const std::string dir = Directory::TempRoot();
    std::string error;
    tools::EnsureDir(dir, error);
#ifdef _WIN32
    const std::string name = "http-" + std::to_string(GetCurrentProcessId()) + "-" +
                             std::to_string(counter.fetch_add(1)) + ".tmp";
#else
    const std::string name = "http-" + std::to_string(counter.fetch_add(1)) + ".tmp";
#endif
    return tools::JoinPath(dir, name);
}

}  // namespace

namespace tools {

std::string ReadTextFile(const std::string& path) {
    std::ifstream stream(tools::ToPath(path), std::ios::binary);
    if (!stream) return {};
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool WriteTextFile(const std::string& path, std::string_view text) {
    std::ofstream stream(tools::ToPath(path), std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}

std::filesystem::path ToPath(const std::string& text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

bool PathExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(tools::ToPath(path), ec);
}

bool IsDirectory(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(tools::ToPath(path), ec);
}

bool EnsureDir(const std::string& path, std::string& error) {
    std::error_code ec;
    fs::create_directories(tools::ToPath(path), ec);
    if (ec) {
        error = "mkdir 失败: " + path;
        return false;
    }
    return true;
}

std::string JoinPath(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    char last = left.back();
    if (last == '/' || last == '\\') return left + right;
    return left + "/" + right;
}

std::string ToLower(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool EndsWith(std::string_view text, std::string_view suffix) {
    if (suffix.size() > text.size()) return false;
    return text.substr(text.size() - suffix.size()) == suffix;
}

std::string MavenToPath(std::string_view name) {
    std::string text(name);
    std::string extension = "jar";
    const size_t at = text.find('@');
    if (at != std::string::npos) {
        extension = text.substr(at + 1);
        text.resize(at);
    }

    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        const size_t pos = text.find(':', start);
        if (pos == std::string::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    if (parts.size() < 3) return {};

    std::string group = parts[0];
    for (char& c : group) {
        if (c == '.') c = '/';
    }
    std::string file = parts[1] + "-" + parts[2];
    if (parts.size() >= 4 && !parts[3].empty()) file += "-" + parts[3];
    file += "." + extension;
    return group + "/" + parts[1] + "/" + parts[2] + "/" + file;
}

std::vector<std::string> ListDirectories(const std::string& path) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(tools::ToPath(path), ec)) {
        if (entry.is_directory()) out.push_back(ToUtf8(entry.path().filename()));
    }
    return out;
}

std::vector<std::string> ListFiles(const std::string& path) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(tools::ToPath(path), ec)) {
        if (entry.is_regular_file()) out.push_back(ToUtf8(entry.path().filename()));
    }
    return out;
}

bool HttpGet(const std::string& url, std::string& out, std::string& error) {
    const std::string temp = TempBodyPath();
    if (!DownloadFile(url, temp, error)) return false;
    out = ReadTextFile(temp);
    std::error_code ec;
    std::filesystem::remove(tools::ToPath(temp), ec);
    return true;
}

namespace {

bool FetchToFile(const std::string& url, const std::string& path,
                 const std::vector<std::string>* headers, std::string& error) {
#ifdef _WIN32
    auto widen = [](const std::string& text) {
        int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring out(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
        return out;
    };

    std::wstring wide_url = widen(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &parts)) {
        error = "bad_url";
        return false;
    }

    std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring target(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength > 0) target.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;

    HINTERNET session = WinHttpOpen(L"rml/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        error = "winhttp_open_failed";
        return false;
    }
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects));

    // 解析/连接/发送/接收都设上限，没有网时别让调用方一直挂着
    WinHttpSetTimeouts(session, 5000, 5000, 10000, 20000);

    HINTERNET connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
    HINTERNET request = connection == nullptr
                            ? nullptr
                            : WinHttpOpenRequest(connection, L"GET", target.c_str(), nullptr,
                                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                 secure ? WINHTTP_FLAG_SECURE : 0);

    bool ok = false;
    if (request != nullptr && headers != nullptr) {
        for (const auto& header : *headers) {
            const std::wstring wide_header = widen(header);
            WinHttpAddRequestHeaders(request, wide_header.c_str(), static_cast<DWORD>(-1),
                                     WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }
    }

    if (request != nullptr &&
        WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0;
        DWORD size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);

        if (status < 200 || status >= 300) {
            error = "http_status_" + std::to_string(status);
        } else {
            DWORD expected = 0;
            DWORD header_size = sizeof(expected);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &expected, &header_size,
                                WINHTTP_NO_HEADER_INDEX);

            uint64_t written = 0;
            std::ofstream stream(tools::ToPath(path), std::ios::binary | std::ios::trunc);
            std::vector<char> buffer(64 * 1024);
            if (!stream) {
                error = "open_failed: " + path;
            } else {
                ok = true;
                while (true) {
                    DWORD available = 0;
                    // QueryDataAvailable 失败不能当 EOF，否则会留下截断文件
                    if (!WinHttpQueryDataAvailable(request, &available)) {
                        error = "read_failed";
                        ok = false;
                        break;
                    }
                    if (available == 0) break;
                    DWORD to_read =
                        available > buffer.size() ? static_cast<DWORD>(buffer.size()) : available;
                    DWORD read = 0;
                    if (!WinHttpReadData(request, buffer.data(), to_read, &read) || read == 0) {
                        error = "read_failed";
                        ok = false;
                        break;
                    }
                    stream.write(buffer.data(), read);
                    written += read;
                    if (!stream) {
                        error = "write_failed";
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    stream.flush();
                    if (!stream) {
                        error = "write_failed";
                        ok = false;
                    }
                }
                if (ok && expected > 0 && written != expected) {
                    error = "download_incomplete";
                    ok = false;
                }
            }
        }
    } else {
        error = "http_request_failed";
    }

    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok;
#else
    (void)url;
    (void)path;
    (void)headers;
    error = "platform_not_supported";
    return false;
#endif
}

}  // namespace

bool DownloadFile(const std::string& url, const std::string& path, std::string& error) {
    return FetchToFile(url, path, nullptr, error);
}

bool HttpGet(const std::string& url, const std::vector<std::string>& headers, std::string& out,
             std::string& error) {
    const std::string temp = TempBodyPath();
    if (!FetchToFile(url, temp, &headers, error)) {
        std::error_code ignored;
        std::filesystem::remove(tools::ToPath(temp), ignored);
        return false;
    }
    out = ReadTextFile(temp);
    std::error_code ec;
    std::filesystem::remove(tools::ToPath(temp), ec);
    return !out.empty();
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

namespace {

inline uint32_t Rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

void Sha1Block(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) w[i] = Rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (int i = 0; i < 80; ++i) {
        uint32_t f = 0, k = 0;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t temp = Rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = Rotl(b, 30);
        b = a;
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

}  // namespace

std::string Sha1Hex(const void* data, size_t size) {
    auto bytes = static_cast<const uint8_t*>(data);
    uint32_t state[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};

    size_t full = size / 64;
    for (size_t i = 0; i < full; ++i) Sha1Block(state, bytes + i * 64);

    uint8_t tail[128] = {0};
    size_t remainder = size % 64;
    for (size_t i = 0; i < remainder; ++i) tail[i] = bytes[full * 64 + i];
    tail[remainder] = 0x80;
    size_t tail_length = (remainder + 1 + 8 <= 64) ? 64 : 128;
    uint64_t bits = static_cast<uint64_t>(size) * 8;
    for (int i = 0; i < 8; ++i) tail[tail_length - 1 - i] = static_cast<uint8_t>((bits >> (i * 8)) & 0xFF);
    for (size_t offset = 0; offset < tail_length; offset += 64) Sha1Block(state, tail + offset);

    static const char* digits = "0123456789abcdef";
    std::string out(40, '0');
    for (int i = 0; i < 5; ++i) {
        uint32_t value = state[i];
        for (int k = 0; k < 4; ++k) {
            uint8_t byte = static_cast<uint8_t>((value >> ((3 - k) * 8)) & 0xFF);
            out[(i * 4 + k) * 2] = digits[byte >> 4];
            out[(i * 4 + k) * 2 + 1] = digits[byte & 0x0F];
        }
    }
    return out;
}

bool Sha1File(const std::string& path, std::string& hex) {
    std::ifstream stream(tools::ToPath(path), std::ios::binary);
    if (!stream) return false;
    std::string data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    hex = Sha1Hex(data.data(), data.size());
    return true;
}

}  // namespace tools

#if defined(_WIN32) && __GNUC__ < 14
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
// GCC 13 的 MinGW 头里没有这两个 time64 声明（GCC 14+ 自带），补一份实现
// 但 -O2 下的 libstdc++ 会引用它们。自己提供实现，别让链接器去 DLL 里找。
extern "C" {

struct RmlTimespec {
    long long tv_sec;
    long tv_nsec;
};

[[used]]
int clock_gettime64(int clock_id, RmlTimespec* ts) {
    if (ts == nullptr) return -1;

    if (clock_id == 0) {  // CLOCK_REALTIME
        FILETIME file_time{};
        GetSystemTimePreciseAsFileTime(&file_time);
        ULARGE_INTEGER ticks{};
        ticks.LowPart = file_time.dwLowDateTime;
        ticks.HighPart = file_time.dwHighDateTime;
        const long long hundred_ns = static_cast<long long>(ticks.QuadPart) - 116444736000000000LL;
        ts->tv_sec = hundred_ns / 10000000LL;
        ts->tv_nsec = static_cast<long>((hundred_ns % 10000000LL) * 100);
        return 0;
    }

    if (clock_id == 1) {  // CLOCK_MONOTONIC
        LARGE_INTEGER frequency{};
        LARGE_INTEGER counter{};
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        ts->tv_sec = counter.QuadPart / frequency.QuadPart;
        ts->tv_nsec = static_cast<long>((counter.QuadPart % frequency.QuadPart) * 1000000000LL /
                                        frequency.QuadPart);
        return 0;
    }

    return -1;
}

[[used]]
int nanosleep64(const RmlTimespec* request, RmlTimespec* remaining) {
    if (request == nullptr) return -1;
    if (remaining != nullptr) {
        remaining->tv_sec = 0;
        remaining->tv_nsec = 0;
    }
    const long long ms = request->tv_sec * 1000 + request->tv_nsec / 1000000;
    Sleep(static_cast<DWORD>(ms > 0 ? ms : 0));
    return 0;
}

}  // extern "C"
#endif