#include "app/tools/tools.h"

#include <cctype>
#include <filesystem>
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

}  // namespace

namespace tools {

std::string ReadTextFile(const std::string& path) {
    std::ifstream stream(fs::u8path(path), std::ios::binary);
    if (!stream) return {};
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool WriteTextFile(const std::string& path, std::string_view text) {
    std::ofstream stream(fs::u8path(path), std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}

bool PathExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(fs::u8path(path), ec);
}

bool IsDirectory(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(fs::u8path(path), ec);
}

bool EnsureDir(const std::string& path, std::string& error) {
    std::error_code ec;
    fs::create_directories(fs::u8path(path), ec);
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

std::vector<std::string> ListDirectories(const std::string& path) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(fs::u8path(path), ec)) {
        if (entry.is_directory()) out.push_back(ToUtf8(entry.path().filename()));
    }
    return out;
}

std::vector<std::string> ListFiles(const std::string& path) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(fs::u8path(path), ec)) {
        if (entry.is_regular_file()) out.push_back(ToUtf8(entry.path().filename()));
    }
    return out;
}

std::string DataRoot() {
#ifdef _WIN32
    char buffer[MAX_PATH * 2];
    DWORD length = GetEnvironmentVariableA("APPDATA", buffer, sizeof(buffer));
    if (length > 0 && length < sizeof(buffer)) {
        return std::string(buffer, length) + "\\RML";
    }
#endif
    return ".";
}

bool HttpGet(const std::string& url, std::string& out, std::string& error) {
    const char* temp_dir = std::getenv("TEMP");
    const std::string temp =
        JoinPath(temp_dir != nullptr ? temp_dir : ".", "rml-http-body.tmp");
    if (!DownloadFile(url, temp, error)) return false;
    out = ReadTextFile(temp);
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(temp), ec);
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
            std::ofstream stream(fs::u8path(path), std::ios::binary | std::ios::trunc);
            std::vector<char> buffer(64 * 1024);
            if (!stream) {
                error = "open_failed: " + path;
            } else {
                ok = true;
                while (true) {
                    DWORD available = 0;
                    if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
                    DWORD to_read =
                        available > buffer.size() ? static_cast<DWORD>(buffer.size()) : available;
                    DWORD read = 0;
                    if (!WinHttpReadData(request, buffer.data(), to_read, &read) || read == 0) {
                        ok = false;
                        break;
                    }
                    stream.write(buffer.data(), read);
                    if (!stream) {
                        ok = false;
                        break;
                    }
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
    const char* temp_dir = std::getenv("TEMP");
    const std::string temp = JoinPath(temp_dir != nullptr ? temp_dir : ".", "rml-http-body.tmp");
    if (!FetchToFile(url, temp, &headers, error)) return false;
    out = ReadTextFile(temp);
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(temp), ec);
    return !out.empty();
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

bool RunAndWait(const std::vector<std::string>& args, std::string& error) {
#ifdef _WIN32
    std::string command_line;
    for (const auto& arg : args) {
        if (!command_line.empty()) command_line.push_back(' ');
        bool quoted = arg.find(' ') != std::string::npos;
        if (quoted) command_line.push_back('"');
        command_line += arg;
        if (quoted) command_line.push_back('"');
    }

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    std::vector<char> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back('\0');

    if (!CreateProcessA(nullptr, mutable_command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startup, &info)) {
        error = "CreateProcess 失败 " + std::to_string(GetLastError());
        return false;
    }
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
    std::ifstream stream(fs::u8path(path), std::ios::binary);
    if (!stream) return false;
    std::string data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    hex = Sha1Hex(data.data(), data.size());
    return true;
}

}  // namespace tools
