#include "app/network/Network.h"
#include "app/directory/Flow/Support.h"

#include <cctype>
#include <filesystem>
#include <vector>

#include "app/directory/Directory.h"
#include "app/json/JsonArena.h"
#include "app/directory/Installer/Version/Version.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace directory {
namespace flow {

namespace {

std::string Trim(std::string text) {
    size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

bool IsHex(const std::string& text) {
    for (char c : text) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

}  // namespace

std::string Support::FileName(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

void Support::Report(const Progress& progress, const std::string& label, int percent) {
    if (progress) progress(label, percent, 100);
}

Progress Support::Segment(const Progress& progress, int from, int to) {
    return [progress, from, to](const std::string& label, int done, int total) {
        if (!progress) return;
        const int percent = total > 0 ? from + (to - from) * done / total : from;
        progress(label, percent, 100);
    };
}

std::string Support::MavenPath(const std::string& coordinate) {
    return tools::MavenToPath(Trim(coordinate));
}

std::string Support::ResolveArtifact(const std::string& libraries_root,
                                     const std::string& coordinate) {
    const std::string relative = MavenPath(coordinate);
    if (relative.empty()) return {};
    return tools::JoinPath(libraries_root, relative);
}

bool Support::ReplaceTokens(const std::map<std::string, std::string>& tokens,
                            const std::string& value, std::string& out, std::string& error) {
    out.clear();
    out.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (c == '\\') {
            if (i + 1 >= value.size()) {
                error = "占位符转义不完整: " + value;
                return false;
            }
            out.push_back(value[++i]);
            continue;
        }
        if (c == '{' || c == '\'') {
            std::string key;
            size_t end = std::string::npos;
            for (size_t j = i + 1; j < value.size(); ++j) {
                const char d = value[j];
                if (d == '\\') {
                    if (j + 1 >= value.size()) {
                        error = "占位符转义不完整: " + value;
                        return false;
                    }
                    key.push_back(value[++j]);
                    continue;
                }
                if (c == '{' && d == '}') {
                    end = j;
                    break;
                }
                if (c == '\'' && d == '\'') {
                    end = j;
                    break;
                }
                key.push_back(d);
            }
            if (end == std::string::npos) {
                error = "占位符没有闭合: " + value;
                return false;
            }
            i = end;
            if (c == '\'') {
                out += key;
                continue;
            }
            auto found = tokens.find(key);
            if (found == tokens.end()) {
                error = "安装器缺少变量 {" + key + "}";
                return false;
            }
            out += found->second;
            continue;
        }
        out.push_back(c);
    }
    return true;
}

std::string Support::RemoteSha1(const std::string& url) {
    std::string text;
    std::string error;
    if (!Network::http(HttpCMD::GET, url, text, error)) return {};
    text = Trim(text);
    if (text.size() != 40 || !IsHex(text)) return {};
    return tools::ToLower(text);
}

bool Support::DownloadFile(const std::string& url, const std::string& dest, const std::string& sha1,
                           std::string& error) {
    if (tools::PathExists(dest)) {
        if (sha1.empty()) return true;
        std::string actual;
        if (tools::Sha1File(dest, actual) && tools::ToLower(actual) == tools::ToLower(sha1)) return true;
    }

    const size_t slash = dest.find_last_of('/');
    if (slash != std::string::npos) {
        std::string dir_error;
        if (!tools::EnsureDir(dest.substr(0, slash), dir_error)) {
            error = dir_error;
            return false;
        }
    }

    const std::string part = dest + ".part";
    std::string download_error;
    if (!Network::download(url, part, download_error)) {
        std::error_code ignored;
        std::filesystem::remove(tools::ToPath(part), ignored);
        error = "下载失败(" + url + "): " + download_error;
        return false;
    }

    if (!sha1.empty()) {
        std::string actual;
        if (!tools::Sha1File(part, actual) || tools::ToLower(actual) != tools::ToLower(sha1)) {
            std::error_code ignored;
            std::filesystem::remove(tools::ToPath(part), ignored);
            error = "下载校验失败: " + url;
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tools::ToPath(part), tools::ToPath(dest), ec);
    if (ec) {
        std::filesystem::remove(tools::ToPath(part), ec);
        error = "落盘失败: " + dest;
        return false;
    }
    return true;
}

bool Support::EnsureVanillaJson(const Context& context, const std::string& mc_version,
                                std::string& error) {
    const Directory paths(context.versions.GameDir());
    const std::string path = paths.VersionJson(mc_version);
    const std::string core_path = paths.CoreJson(paths.CoreForEntry(mc_version));

    for (const std::string& candidate : {path, core_path}) {
        if (!tools::PathExists(candidate)) continue;
        Json::Value root;
        Json json;
        std::string parse_error;
        if (json.Parse(Directory::ReadTextFile(candidate), root, parse_error) &&
            !root["id"].AsString().empty()) {
            return true;
        }
    }

    std::vector<download::versionsource::Mojang::Entry> entries;
    if (!context.sources.Game().List(entries, error)) return false;

    std::string url;
    for (const auto& entry : entries) {
        if (entry.id == mc_version) {
            url = entry.url;
            break;
        }
    }
    if (url.empty()) {
        error = "Mojang 清单里没有 " + mc_version;
        return false;
    }

    std::string text;
    if (!context.sources.Game().Fetch(url, text, error)) return false;
    if (!installer::Version::WriteVersionJson(context.versions.GameDir(), mc_version, text, error)) {
        return false;
    }
    return true;
}

bool Support::EnsureClientJar(const Context& context, const std::string& mc_version,
                              const Progress& progress, std::string& error) {
    const Directory paths(context.versions.GameDir());
    const std::string jar_path = paths.VersionJar(mc_version);
    if (tools::PathExists(jar_path) ||
        tools::PathExists(paths.CoreJar(paths.CoreForEntry(mc_version)))) {
        return true;
    }

    const std::string json_path = paths.VersionJson(mc_version);
    Json::Value root;
    Json json;
    std::string parse_error;
    if (!json.Parse(Directory::ReadTextFile(json_path), root, parse_error)) {
        error = "读不到原版版本描述: " + json_path;
        return false;
    }

    const Json::Value& client = root["downloads"]["client"];
    const std::string url = client["url"].AsString();
    if (url.empty()) {
        error = "原版描述里没有客户端 jar 的下载地址: " + mc_version;
        return false;
    }

    installer::Transfer::Task task;
    task.url = url;
    task.sha1 = client["sha1"].AsString();
    task.dest = jar_path;
    task.label = mc_version + ".jar";
    task.size = client["size"].AsInt(0);

    installer::Transfer::Result result;
    return context.installer.Transfers().Run({task}, progress, result, error);
}

bool Support::ReadMainClass(const std::string& jar, const std::string& work_dir,
                            std::string& main_class, std::string& error) {
    std::string dir_error;
    if (!tools::EnsureDir(work_dir, dir_error)) {
        error = dir_error;
        return false;
    }

    const std::string manifest = tools::JoinPath(work_dir, "MANIFEST.MF");
    std::error_code ignored;
    std::filesystem::remove(tools::ToPath(manifest), ignored);

    std::string extracted;
    if (!tools::ExtractEntry(jar, "META-INF/MANIFEST.MF", work_dir, extracted, error)) {
        error = "读不到处理器 jar 的 MANIFEST: " + jar;
        return false;
    }

    const std::string text = Directory::ReadTextFile(extracted);
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        std::string line = end == std::string::npos ? text.substr(start) : text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line[0] == ' ' && !lines.empty()) {
            lines.back() += line.substr(1);
        } else {
            lines.push_back(line);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }

    const std::string prefix = "main-class:";
    for (const auto& line : lines) {
        if (tools::ToLower(line).rfind(prefix, 0) != 0) continue;
        main_class = Trim(line.substr(prefix.size()));
        if (!main_class.empty()) return true;
    }

    error = "处理器 jar 的 MANIFEST 里没有 Main-Class: " + jar;
    return false;
}

bool Support::WriteBaseMarker(const std::string& game_dir, const std::string& mc_version,
                              std::string& error) {
    const Directory paths(game_dir);
    const std::string dir = paths.VersionDir(mc_version);
    if (tools::PathExists(dir + "/rml.json")) return true;  // 用户已有原版，保持原状

    auto j = JsonArena::Instance().Open(dir + "/rml.json");
    if (!j) {
        error = j.error();
        return false;
    }
    j["schema"] = Json::Value::Int(2);
    j["id"] = Json::Value::String(mc_version);
    j["name"] = Json::Value::String(mc_version);
    j["role"] = Json::Value::String("base");
    j["loader"] = Json::Value::String("原版");
    j["loaderVersion"] = Json::Value::String("");
    j["minecraft"] = Json::Value::String(mc_version);
    j["base"] = Json::Value::String("");
    j["complete"] = Json::Value::Bool(true);
    j["userInstalled"] = Json::Value::Bool(false);
    j["state"] = Json::Value::String("committed");
    j["fresh"] = Json::Value::Bool(false);
    return j.commit(error);
}

}  // namespace flow
}  // namespace directory
