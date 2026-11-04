#include "app/launch/Environment/Environment.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "app/directory/Directory.h"
#include "app/tools/tools.h"

#ifdef _WIN32
#include <windows.h>

#include <cctype>
#endif

namespace launch {

namespace {

int ParseMajorFromTokens(const std::string& text) {
    // 从 jdk-21.0.10 / jre1.8.0_471 / 1.8.0_202 这类串里取主版本
    for (size_t i = 0; i < text.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) continue;
        size_t start = i;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
        std::string number = text.substr(start, i - start);
        if (number == "1") {
            // 1.8.0_471 → 8
            if (i < text.size() && text[i] == '.') {
                size_t j = i + 1;
                size_t begin = j;
                while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) ++j;
                if (j > begin) return std::atoi(text.substr(begin, j - begin).c_str());
            }
            continue;
        }
        int value = std::atoi(number.c_str());
        if (value >= 7 && value <= 99) return value;
    }
    return 0;
}

#ifdef _WIN32

std::string Narrow(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
                                   nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr,
                        nullptr);
    return out;
}

std::wstring Widen(const std::string& text) {
    if (text.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

void CollectJavaHomes(HKEY root, const wchar_t* subkey, std::vector<std::string>& homes) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) return;

    wchar_t name[256];
    DWORD index = 0;
    while (true) {
        DWORD length = 256;
        if (RegEnumKeyExW(key, index++, name, &length, nullptr, nullptr, nullptr, nullptr) !=
            ERROR_SUCCESS) {
            break;
        }
        HKEY version_key = nullptr;
        if (RegOpenKeyExW(key, name, 0, KEY_READ | KEY_WOW64_64KEY, &version_key) != ERROR_SUCCESS) {
            continue;
        }
        wchar_t home[MAX_PATH];
        DWORD home_size = sizeof(home);
        DWORD type = 0;
        if (RegQueryValueExW(version_key, L"JavaHome", nullptr, &type,
                             reinterpret_cast<LPBYTE>(home), &home_size) == ERROR_SUCCESS &&
            type == REG_SZ) {
            homes.push_back(Narrow(home));
        }
        RegCloseKey(version_key);
    }
    RegCloseKey(key);
}

std::string GetEnv(const char* name) {
    char buffer[MAX_PATH * 2];
    DWORD length = GetEnvironmentVariableA(name, buffer, sizeof(buffer));
    if (length == 0 || length >= sizeof(buffer)) return {};
    return std::string(buffer, length);
}

// 跑一次 java -version，从 stderr 解析版本号
int ProbeJavaVersion(const std::string& path, std::string& version) {
    std::string command = "\"" + path + "\" -version 2>&1";
    FILE* pipe = _popen(command.c_str(), "r");
    if (pipe == nullptr) return 0;

    char line[512];
    while (std::fgets(line, sizeof(line), pipe) != nullptr) {
        std::string text(line);
        size_t quote_begin = text.find("version \"");
        if (quote_begin == std::string::npos) continue;
        size_t value_begin = quote_begin + 9;
        size_t quote_end = text.find('"', value_begin);
        if (quote_end == std::string::npos) continue;
        version = text.substr(value_begin, quote_end - value_begin);
        break;
    }
    _pclose(pipe);
    return ParseMajorFromTokens(version);
}

#endif  // _WIN32

}  // namespace

Environment::Environment() { Probe(); }

void Environment::AddCandidate(const std::string& path, const std::string& source) {
    if (path.empty() || !tools::PathExists(path)) return;

    // 同一个 java 可能从 PATH 和目录扫描各来一次，反斜杠/正斜杠也算同一个
    auto normalize = [](const std::string& text) {
        std::string out = tools::ToLower(text);
        std::replace(out.begin(), out.end(), '\\', '/');
        return out;
    };
    const std::string lowered = normalize(path);
    for (const auto& java : javas_) {
        if (normalize(java.path) == lowered) return;
    }

    Java java;
    java.path = path;
    java.source = source;
    java.major = ParseMajorFromTokens(path);

#ifdef _WIN32
    if (java.major == 0) {
        java.major = ProbeJavaVersion(path, java.version);
    }
#endif
    if (java.major == 0) return;  // 不是可用的 java

    javas_.push_back(std::move(java));
}

void Environment::Probe() {
#ifdef _WIN32
    // 0. RML 自己装的优先：%APPDATA%\RML\runtimes\<名字>\bin\java.exe
    const std::string managed = ManagedRoot();
    if (tools::IsDirectory(managed)) {
        for (const auto& child : tools::ListDirectories(managed)) {
            AddCandidate(tools::JoinPath(tools::JoinPath(tools::JoinPath(managed, child), "bin"), "java.exe"),
                         "RML");
        }
    }

    // 1. PATH
    char buffer[MAX_PATH * 2];
    DWORD length = SearchPathA(nullptr, "java.exe", nullptr, sizeof(buffer), buffer, nullptr);
    if (length > 0) AddCandidate(std::string(buffer, length), "PATH");

    // 2. JAVA_HOME
    std::string java_home = GetEnv("JAVA_HOME");
    if (!java_home.empty()) {
        AddCandidate(tools::JoinPath(tools::JoinPath(java_home, "bin"), "java.exe"), "JAVA_HOME");
    }

    // 3. 注册表
    std::vector<std::string> homes;
    CollectJavaHomes(HKEY_LOCAL_MACHINE, L"SOFTWARE\\JavaSoft\\JDK", homes);
    CollectJavaHomes(HKEY_LOCAL_MACHINE, L"SOFTWARE\\JavaSoft\\Java Runtime Environment", homes);
    for (const auto& home : homes) {
        AddCandidate(tools::JoinPath(tools::JoinPath(home, "bin"), "java.exe"), "注册表");
    }

    // 4. 常见安装目录
    const std::string program_files = GetEnv("ProgramFiles");
    const std::string local_app_data = GetEnv("LOCALAPPDATA");
    const std::vector<std::pair<std::string, std::string>> roots = {
        {tools::JoinPath(program_files, "Java"), "Java"},
        {tools::JoinPath(program_files, "Microsoft"), "Microsoft"},
        {tools::JoinPath(program_files, "Eclipse Adoptium"), "Adoptium"},
        {tools::JoinPath(program_files, "Zulu"), "Zulu"},
        {tools::JoinPath(program_files, "Amazon Corretto"), "Corretto"},
        {tools::JoinPath(program_files, "BellSoft"), "BellSoft"},
        {tools::JoinPath(local_app_data, "Programs"), "用户目录"},
    };
    for (const auto& [root, source] : roots) {
        if (!tools::IsDirectory(root)) continue;
        for (const auto& child : tools::ListDirectories(root)) {
            AddCandidate(tools::JoinPath(tools::JoinPath(tools::JoinPath(root, child), "bin"), "java.exe"),
                         source);
        }
    }
#endif

    std::sort(javas_.begin(), javas_.end(),
              [](const Java& a, const Java& b) { return a.major > b.major; });
}

const Environment::Java* Environment::Pick(int required_major) const {
    for (const auto& java : javas_) {
        if (java.major == required_major) return &java;
    }

    // 没有正好匹配的，选不低于要求的最低版本：Java 17 的版本用 21 跑通常没问题
    const Java* best = nullptr;
    for (const auto& java : javas_) {
        if (java.major < required_major) continue;
        if (best == nullptr || java.major < best->major) best = &java;
    }
    return best;
}

const Environment::Java* Environment::Find(const std::string& path) const {
    for (const auto& java : javas_) {
        if (java.path == path) return &java;
    }
    return nullptr;
}

std::string Environment::ManagedRoot() { return Directory::RuntimesRoot(); }

void Environment::Refresh() {
    javas_.clear();
    Probe();
}

bool Environment::Install(int major, std::string& error) {
    const std::string root = ManagedRoot();
    std::string dir_error;
    if (!tools::EnsureDir(root, dir_error)) {
        error = dir_error;
        return false;
    }

    const char* temp = std::getenv("TEMP");
    const std::string archive =
        tools::JoinPath(temp != nullptr ? temp : ".", "rml-jre-" + std::to_string(major) + ".zip");
    const std::string base = "https://api.adoptium.net/v3/binary/latest/" +
                             std::to_string(major) + "/ga/windows/x64/";

    auto download = [&](const char* kind) {
        error.clear();
        return tools::DownloadFile(base + kind + "/hotspot/normal/eclipse", archive, error);
    };

    // JRE 优先；某些大版本没有 JRE 包时退到 JDK
    if (!download("jre")) {
        if (error.find("http_status_404") == std::string::npos) {
            error = "下载 Java " + std::to_string(major) + " 失败: " + error;
            return false;
        }
        if (!download("jdk")) {
            error = "下载 Java " + std::to_string(major) + " 失败: " + error;
            return false;
        }
    }

    // 解压到临时目录，避免压缩包没有顶层目录时污染 runtimes
    namespace fs = std::filesystem;
    const std::string staging = tools::JoinPath(root, "extract-" + std::to_string(major));
    std::error_code ec;
    fs::remove_all(tools::ToPath(staging), ec);
    if (!tools::EnsureDir(staging, error)) return false;

    if (!tools::RunAndWait({"tar", "-xf", archive, "-C", staging}, error)) {
        error = "解压失败: " + error;
        return false;
    }

    // Adoptium 包顶层通常是 jdk8u...-jre / jdk-17...，也可能是压缩包根目录
    std::string source;
    if (tools::PathExists(tools::JoinPath(staging, "bin/java.exe"))) {
        source = staging;
    } else {
        for (const auto& child : tools::ListDirectories(staging)) {
            const std::string candidate = tools::JoinPath(staging, child);
            if (tools::PathExists(tools::JoinPath(candidate, "bin/java.exe"))) {
                source = candidate;
                break;
            }
        }
    }
    if (source.empty()) {
        error = "Java 压缩包解压后找不到 bin/java.exe";
        return false;
    }

    const std::string target = tools::JoinPath(root, "temurin-" + std::to_string(major));
    fs::remove_all(tools::ToPath(target), ec);
    fs::rename(tools::ToPath(source), tools::ToPath(target), ec);
    if (ec) {
        error = "安装 Java 失败：无法移动解压目录";
        return false;
    }
    fs::remove_all(tools::ToPath(staging), ec);
    fs::remove(tools::ToPath(archive), ec);

    Refresh();
    return tools::PathExists(tools::JoinPath(target, "bin/java.exe"));
}

}  // namespace launch
