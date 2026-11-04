#include "app/server/Installer/Installer.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "app/directory/Directory.h"
#include "app/download/VersionSource/Forge/Forge.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace server {

namespace {

bool LoadVanillaJson(download::VersionSource& sources, const std::string& mc_version,
                     Json::Value& root, std::string& error) {
    std::vector<download::versionsource::Mojang::Entry> entries;
    if (!sources.Game().List(entries, error)) {
        error = "拉取版本清单失败: " + error;
        return false;
    }
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
    if (!sources.Game().Fetch(url, text, error)) return false;
    Json json;
    std::string parse_error;
    if (!json.Parse(text, root, parse_error)) {
        error = "解析 " + mc_version + " 版本描述失败: " + parse_error;
        return false;
    }
    return true;
}

std::string FileNameFromUrl(const std::string& url) {
    const size_t slash = url.find_last_of('/');
    return slash == std::string::npos ? url : url.substr(slash + 1);
}

std::string FindRelativeFile(const std::string& root, const std::string& filename) {
    std::error_code ec;
    const std::u8string wanted(filename.begin(), filename.end());
    for (const auto& entry : std::filesystem::recursive_directory_iterator(tools::ToPath(root), ec)) {
        if (ec || !entry.is_regular_file()) continue;
        if (entry.path().filename().u8string() != wanted) continue;
        const auto relative = std::filesystem::relative(entry.path(), tools::ToPath(root), ec);
        if (ec) continue;
        const auto u8 = relative.generic_u8string();
        return std::string(u8.begin(), u8.end());
    }
    return {};
}

std::string FindUniversalJar(const std::string& root) {
    std::error_code ec;
    // Forge 1.12.2 的安装器直接生成 forge-<mc>-<version>.jar，没有 universal 后缀；
    // 先看安装根目录，避免误扫 libraries。
    for (const auto& entry : std::filesystem::directory_iterator(tools::ToPath(root), ec)) {
        if (ec || !entry.is_regular_file()) continue;
        const auto name_u8 = entry.path().filename().u8string();
        const std::string name(name_u8.begin(), name_u8.end());
        if (name.size() < 4 || name.substr(name.size() - 4) != ".jar") continue;
        if (name.rfind("forge-", 0) != 0) continue;
        if (name.find("-installer") != std::string::npos) continue;
        const auto u8 = entry.path().generic_u8string();
        return std::string(u8.begin(), u8.end());
    }

    ec.clear();
    for (const auto& entry : std::filesystem::recursive_directory_iterator(tools::ToPath(root), ec)) {
        if (ec || !entry.is_regular_file()) continue;
        const auto name_u8 = entry.path().filename().u8string();
        const std::string name(name_u8.begin(), name_u8.end());
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".jar" &&
            name.find("-universal") != std::string::npos) {
            const auto u8 = entry.path().generic_u8string();
            return std::string(u8.begin(), u8.end());
        }
    }
    return {};
}

struct InstallCleanup {
    std::string dir;
    bool keep = false;
    ~InstallCleanup() {
        if (keep) return;
        std::error_code ec;
        std::filesystem::remove_all(tools::ToPath(dir), ec);
    }
};

}  // namespace

bool Installer::Forge(const std::string& mc_version, const Progress& progress,
                      std::string& version_id, std::string& error) const {
    std::string loader_version;
    if (!sources_.Forges().Latest(mc_version, loader_version, error)) {
        error = "获取 " + mc_version + " 的 Forge 版本失败: " + error;
        return false;
    }
    if (loader_version.empty()) {
        error = "找不到 " + mc_version + " 可用的 Forge 版本";
        return false;
    }

    Json::Value vanilla;
    if (!LoadVanillaJson(sources_, mc_version, vanilla, error)) return false;
    int java_major = static_cast<int>(vanilla["javaVersion"]["majorVersion"].AsInt(8));
    if (java_major <= 0) java_major = 8;

    const std::string id = mc_version + "-forge-" + loader_version + "-server";
    const std::string name = mc_version + " Forge " + loader_version + " 服务端";
    if (!Prepare(id, name, "Forge", mc_version, java_major, error)) return false;

    const std::string installer_url = download::versionsource::Forge::InstallerUrl(
        sources_.Origins().ForgeRoot(), loader_version);
    if (installer_url.empty()) {
        error = "Forge 安装器地址为空";
        return false;
    }

    const std::string cache_dir = Directory::InstallerCacheDir();
    std::string dir_error;
    if (!tools::EnsureDir(cache_dir, dir_error)) {
        error = dir_error;
        return false;
    }
    const std::string installer_path =
        tools::JoinPath(cache_dir, "forge-server-" + FileNameFromUrl(installer_url));

    directory::installer::Transfer::Task task;
    task.url = installer_url;
    task.dest = installer_path;
    task.label = "Forge 服务端安装器 " + loader_version;
    directory::installer::Transfer::Result result;
    if (!transfer_.Run({task}, progress, result, error)) {
        error = "下载 Forge 服务端安装器失败: " + error;
        return false;
    }

    const launch::Environment::Java* java = environment_.Pick(java_major);
    if (java == nullptr) {
        error = "安装 Forge 服务端需要 Java " + std::to_string(java_major) + "，机器上没有可用的";
        return false;
    }

    const std::string dir = versions_.VersionDir(id);
    InstallCleanup cleanup{dir, false};
    const std::string log_path = tools::JoinPath(dir, "forge-install.log");
    if (!tools::RunAndWait({java->path, "-jar", installer_path, "--installServer", dir}, log_path,
                           error)) {
        error = "Forge 服务端安装器执行失败（日志: " + log_path + "）: " + error;
        return false;
    }

    const std::string args_file = FindRelativeFile(dir, "win_args.txt");
    if (!args_file.empty()) {
        if (!WriteServerMarker(versions_, id, name, "Forge", mc_version, java_major, std::string(),
                               {}, args_file, error)) {
            return false;
        }
        cleanup.keep = true;
        version_id = id;
        return true;
    }

    // 老版本 Forge（如 1.12.2）生成 universal jar；复制成统一的 server.jar。
    const std::string universal = FindUniversalJar(dir);
    if (universal.empty()) {
        error = "Forge 安装器没有生成 win_args.txt 或 universal jar";
        return false;
    }
    const std::string server_jar = tools::JoinPath(dir, "server.jar");
    std::error_code ec;
    std::filesystem::remove(tools::ToPath(server_jar), ec);
    ec.clear();
    std::filesystem::create_hard_link(tools::ToPath(universal), tools::ToPath(server_jar), ec);
    if (ec) {
        ec.clear();
        std::filesystem::copy_file(tools::ToPath(universal), tools::ToPath(server_jar),
                                   std::filesystem::copy_options::overwrite_existing, ec);
    }
    if (ec) {
        error = "复制 Forge universal jar 失败: " + universal;
        return false;
    }

    if (!WriteServerMarker(versions_, id, name, "Forge", mc_version, java_major, std::string(), {},
                           std::string(), error)) {
        return false;
    }
    cleanup.keep = true;
    version_id = id;
    return true;
}

}  // namespace server
