#include "app/download/Flow/Profile.h"

#include <cctype>
#include <filesystem>
#include <vector>

#include "app/download/Flow/Processors.h"
#include "app/download/Flow/Support.h"
#include "app/download/Installer/Version/Version.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace download {
namespace flow {

namespace {

std::string SafeName(std::string text) {
    for (char& c : text) {
        const bool keep = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                          (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '_';
        if (!keep) c = '_';
    }
    return text;
}

bool CopyFile(const std::string& from, const std::string& to, std::string& error) {
    const size_t slash = to.find_last_of('/');
    if (slash != std::string::npos) {
        std::string dir_error;
        if (!tools::EnsureDir(to.substr(0, slash), dir_error)) {
            error = dir_error;
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::copy_file(tools::ToPath(from), tools::ToPath(to),
                               std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        error = "复制文件失败: " + from + " -> " + to;
        return false;
    }
    return true;
}

bool PickJava(const Context& context, const std::string& mc_version, std::string& java,
              std::string& error) {
    const std::string json_path = tools::JoinPath(
        tools::JoinPath(tools::JoinPath(context.versions.GameDir(), "versions"), mc_version),
        mc_version + ".json");

    int major = 8;
    Json::Value root;
    Json json;
    std::string parse_error;
    if (json.Parse(tools::ReadTextFile(json_path), root, parse_error) &&
        root["javaVersion"].IsObject()) {
        major = static_cast<int>(root["javaVersion"]["majorVersion"].AsInt(8));
    }

    const launch::Environment::Java* picked = context.environment.Pick(major);
    if (picked == nullptr) {
        error = "安装 " + mc_version + " 需要 Java " + std::to_string(major) + "，机器上没有可用的";
        return false;
    }
    java = picked->path;
    return true;
}

// 版本 json 里 url 为空的库：从安装器的 maven/ 目录取；取不到的先放过（处理器可能生成）
bool ExtractEmbeddedLibraryList(const std::string& installer_jar, const std::string& libraries_root,
                                const Json::Value& libraries, const std::string& work_dir,
                                std::string& error) {
    for (size_t i = 0; i < libraries.Size(); ++i) {
        const Json::Value& artifact = libraries.At(i)["downloads"]["artifact"];
        const std::string path = artifact["path"].AsString();
        const std::string url = artifact["url"].AsString();
        if (path.empty() || !url.empty()) continue;

        const std::string dest = tools::JoinPath(libraries_root, path);
        if (tools::PathExists(dest)) continue;

        std::string extracted;
        std::string ignored;
        if (tools::ExtractEntry(installer_jar, "maven/" + path, work_dir, extracted, ignored)) {
            if (!CopyFile(extracted, dest, error)) return false;
        }
    }
    return true;
}

bool ExtractEmbeddedLibraries(const std::string& installer_jar, const std::string& libraries_root,
                              const std::string& version_json, const std::string& work_dir,
                              std::string& error) {
    Json::Value root;
    Json json;
    std::string parse_error;
    if (!json.Parse(version_json, root, parse_error)) {
        error = "版本描述解析失败: " + parse_error;
        return false;
    }
    return ExtractEmbeddedLibraryList(installer_jar, libraries_root, root["libraries"], work_dir,
                                      error);
}

bool VerifyEmptyUrlLibraries(const std::string& libraries_root, const std::string& version_json,
                             std::string& error) {
    Json::Value root;
    Json json;
    std::string parse_error;
    if (!json.Parse(version_json, root, parse_error)) return true;

    std::vector<std::string> missing;
    const Json::Value& libraries = root["libraries"];
    for (size_t i = 0; i < libraries.Size(); ++i) {
        const Json::Value& artifact = libraries.At(i)["downloads"]["artifact"];
        const std::string path = artifact["path"].AsString();
        const std::string url = artifact["url"].AsString();
        if (path.empty() || !url.empty()) continue;
        if (!tools::PathExists(tools::JoinPath(libraries_root, path))) missing.push_back(path);
    }

    if (missing.empty()) return true;
    std::string text;
    for (const auto& path : missing) {
        if (!text.empty()) text += "\n· ";
        text += path;
    }
    error = "安装器没有提供、处理器也没有生成这些库：\n· " + text;
    return false;
}

}  // namespace

bool Profile::Install(const Context& context, const Request& request, const std::string& loader,
                      const std::string& maven_version, const std::string& installer_url,
                      const Progress& progress, Outcome& outcome, std::string& error) {
    const std::string game_dir = request.game_dir;
    const std::string libraries_root = tools::JoinPath(game_dir, "libraries");
    const std::string cache_dir = tools::JoinPath(tools::DataRoot(), "cache/installers");
    const std::string temp_root = tools::JoinPath(
        tools::JoinPath(tools::DataRoot(), "tmp"), SafeName(loader) + "-" + SafeName(maven_version));

    std::string dir_error;
    if (!tools::EnsureDir(cache_dir, dir_error) || !tools::EnsureDir(temp_root, dir_error)) {
        error = dir_error;
        return false;
    }

    auto report = [&progress](const std::string& label, int percent) {
        if (progress) progress(label, percent, 100);
    };
    auto segment = [&progress](int from, int to) {
        return [&progress, from, to](const std::string& label, int done, int total) {
            if (!progress) return;
            const int percent = total > 0 ? from + (to - from) * done / total : from;
            progress(label, percent, 100);
        };
    };

    // —— 原版底座：版本描述 + 客户端 jar（处理器要拿它当输入） ——
    report("准备原版 " + request.mc_version, 3);
    if (!Support::EnsureVanillaJson(context, request.mc_version, error)) return false;
    auto client_progress = segment(3, 8);
    if (!Support::EnsureClientJar(context, request.mc_version, client_progress, error)) return false;

    // —— 安装器 ——
    report("下载 " + loader + " 安装器", 10);
    const std::string installer_path =
        tools::JoinPath(cache_dir, Support::FileName(installer_url));
    const std::string sha1 = Support::RemoteSha1(installer_url + ".sha1");
    if (!Support::DownloadFile(installer_url, installer_path, sha1, error)) {
        error = loader + " 安装器下载失败：" + error;
        return false;
    }

    report("解包安装器", 14);
    const std::string profile_dir = tools::JoinPath(temp_root, "profile");
    std::string profile_path;
    if (!tools::ExtractEntry(installer_path, "install_profile.json", profile_dir, profile_path,
                             error)) {
        return false;
    }

    Json::Value profile;
    Json json;
    std::string parse_error;
    if (!json.Parse(tools::ReadTextFile(profile_path), profile, parse_error)) {
        error = "install_profile.json 解析失败: " + parse_error;
        return false;
    }

    const bool legacy = profile["versionInfo"].IsObject();
    std::string version_json;
    std::string version_id;

    if (legacy) {
        version_json = profile["versionInfo"].Dump();
        version_id = profile["versionInfo"]["id"].AsString();
        if (version_id.empty()) version_id = profile["install"]["target"].AsString();
    } else {
        std::string entry = profile["json"].AsString();
        if (entry.empty()) entry = "/version.json";
        if (!entry.empty() && entry.front() == '/') entry.erase(entry.begin());

        std::string version_path;
        if (!tools::ExtractEntry(installer_path, entry, profile_dir, version_path, error)) return false;
        version_json = tools::ReadTextFile(version_path);

        Json::Value version_root;
        Json version_parser;
        if (!version_parser.Parse(version_json, version_root, parse_error)) {
            error = "安装器里的 version.json 解析失败: " + parse_error;
            return false;
        }
        version_id = version_root["id"].AsString();
        if (version_id.empty()) version_id = profile["version"].AsString();
    }

    if (version_id.empty()) {
        error = loader + " 安装器里没有版本 id";
        return false;
    }

    report("写入版本描述", 18);
    if (!installer::Version::WriteMarker(game_dir, version_id, loader, false)) {
        error = "写托管标记失败: " + version_id;
        return false;
    }
    if (!installer::Version::WriteVersionJson(game_dir, version_id, version_json, error)) return false;

    // 老格式（1.13 以前）：universal jar 在安装器根目录，按 install.path 放进 libraries
    if (legacy) {
        const std::string file_path = profile["install"]["filePath"].AsString();
        const std::string artifact = profile["install"]["path"].AsString();
        const std::string dest = Support::ResolveArtifact(libraries_root, artifact);
        if (!dest.empty() && !tools::PathExists(dest)) {
            std::string extracted;
            if (!tools::ExtractEntry(installer_path, file_path,
                                     tools::JoinPath(temp_root, "legacy"), extracted, error)) {
                return false;
            }
            if (!CopyFile(extracted, dest, error)) return false;
        }
    } else if (!ExtractEmbeddedLibraries(installer_path, libraries_root, version_json,
                                         tools::JoinPath(temp_root, "maven"), error)) {
        return false;
    } else if (!ExtractEmbeddedLibraryList(installer_path, libraries_root, profile["libraries"],
                                           tools::JoinPath(temp_root, "maven"), error)) {
        // Forge 1.16.5 的 universal jar 只出现在 install_profile.libraries 里，
        // 版本 json 只引用无 classifier 的 shim；FML 启动时会去找 -universal.jar。
        return false;
    }

    // —— 依赖库（处理器 jar + universal） ——
    report("下载依赖库", 22);
    Processors processors(context.installer.Transfers());
    auto library_progress = segment(22, 45);
    if (!processors.DownloadLibraries(libraries_root, profile, library_progress, error)) return false;

    // —— 处理器 ——
    const bool has_processors =
        profile["processors"].IsArray() && profile["processors"].Size() > 0;
    if (has_processors) {
        std::string java;
        if (!PickJava(context, request.mc_version, java, error)) return false;

        auto processor_progress = segment(45, 75);
        if (!processors.Run(java, game_dir, installer_path, request.mc_version, temp_root, profile,
                            processor_progress, error)) {
            return false;
        }
    }

    if (!legacy && !VerifyEmptyUrlLibraries(libraries_root, version_json, error)) return false;

    // —— 全量补全：客户端 jar / 库 / 资源 / natives ——
    report("下载游戏文件与资源", 78);
    context.versions.Refresh();
    if (!context.versions.Select(version_id)) {
        error = "装完扫描不到版本: " + version_id;
        return false;
    }

    auto install_progress = segment(78, 100);
    installer::Transfer::Result result;
    if (!context.installer.Versions().Install(context.installer.Transfers(), install_progress, result,
                                              error, true, context.sources.Origins().AssetsRoot())) {
        error = loader + " 安装未完成：" + error;
        return false;
    }

    installer::Version::WriteMarker(game_dir, version_id, loader, true);
    installer::Version::WriteMarker(game_dir, request.mc_version, "原版", true);

    outcome.version_id = version_id;
    outcome.loader_version = maven_version;
    return true;
}

}  // namespace flow
}  // namespace download
