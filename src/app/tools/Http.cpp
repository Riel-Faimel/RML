// 网络传输：WinHTTP 下载与取文本。
#include "app/directory/Directory.h"
#include "app/tools/tools.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace {

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

namespace tools {

bool DownloadFile(const std::string& url, const std::string& path, std::string& error) {
    return FetchToFile(url, path, nullptr, error);
}

bool HttpGet(const std::string& url, std::string& out, std::string& error) {
    const std::string temp = TempBodyPath();
    if (!DownloadFile(url, temp, error)) return false;
    out = ReadTextFile(temp);
    std::error_code ec;
    std::filesystem::remove(tools::ToPath(temp), ec);
    return true;
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

}  // namespace tools
