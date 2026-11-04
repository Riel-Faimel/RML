// 路径与文本文件：tools 模块里不依赖网络/进程的部分。
#include "app/tools/tools.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace {

std::string ToUtf8(const fs::path& path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
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

}  // namespace tools
