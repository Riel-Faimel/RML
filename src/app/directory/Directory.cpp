#include "app/directory/Directory.h"
#include "app/directory/Flow/Dispatcher.h"
#include "app/json/JsonArena.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <utility>

#include "app/tools/Json.h"
#include "app/tools/tools.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::string Join(const std::string& parent, const std::string& child) {
    if (parent.empty()) return child;
    if (child.empty()) return parent;
    if (parent.back() == '/' || parent.back() == '\\') return parent + child;
    return parent + "/" + child;
}

std::filesystem::path ToPath(const std::string& text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

}  // namespace

Directory::Directory(std::string game_root) : game_root_(std::move(game_root)) {}

std::string Directory::GameFile(const std::string& name) const {
    return Join(game_root_, name);
}

Directory::InstallTarget Directory::Install(const std::string& version_id) const {
    InstallTarget target;
    target.client_jar = VersionJar(version_id);
    target.libraries_root = LibrariesRoot();
    target.asset_indexes_root = AssetIndexesRoot();
    target.asset_objects_root = AssetObjectsRoot();
    return target;
}

bool Directory::Install(const std::string& installer_type_id,
                        const directory::flow::Context& context,
                        const directory::flow::Request& request,
                        const std::function<void(const std::string&, int, int)>& progress,
                        directory::flow::Outcome& outcome, std::string& error) const {
    std::string loader = tools::ToLower(installer_type_id);
    if (loader == "vanilla" || loader == "原版") loader = "原版";
    else if (loader == "fabric") loader = "Fabric";
    else if (loader == "forge") loader = "Forge";
    else if (loader == "neoforge") loader = "NeoForge";

    directory::flow::Request actual = request;
    actual.loader = loader;
    if (!directory::flow::Dispatcher().Run(context, actual, progress, outcome, error)) {
        return false;
    }

    // 安装完成后立刻把核心文件搬进 cores；失败不阻断，入口仍能用自己的文件启动。
    MigrationReport report;
    MigrateCores(false, report, outcome.version_id);
    return true;
}

std::string Directory::InstallTarget::Library(const std::string& relative) const {
    return tools::JoinPath(libraries_root, relative);
}

std::string Directory::InstallTarget::AssetObject(const std::string& hash) const {
    if (hash.size() < 2) return {};
    return tools::JoinPath(tools::JoinPath(asset_objects_root, hash.substr(0, 2)), hash);
}

std::string Directory::InstallTarget::AssetIndex(const std::string& id) const {
    return tools::JoinPath(asset_indexes_root, id + ".json");
}

namespace {

bool WriteTextEnsured(const std::string& path, std::string_view text) {
    const size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) {
        std::string error;
        if (!tools::EnsureDir(path.substr(0, slash), error)) return false;
    }
    return tools::WriteTextFile(path, text);
}

}  // namespace

bool Directory::ReadVersionText(const std::string& id, const std::string& relative,
                                std::string& out) const {
    const std::string path = Join(VersionDir(id), relative);
    if (!tools::PathExists(path)) return false;
    out = tools::ReadTextFile(path);
    return !out.empty();
}

bool Directory::WriteVersionText(const std::string& id, const std::string& relative,
                                 std::string_view text) const {
    return WriteTextEnsured(Join(VersionDir(id), relative), text);
}

bool Directory::EnsureRepositoryIndex() const {
    const std::string repo = RepositoryModsRoot();
    if (!tools::IsDirectory(repo)) return false;

    const std::string index_path = Join(repo, "rml.json");
    auto index = JsonArena::Instance().Open(index_path);
    if (!index) return false;

    constexpr std::string_view kSuffix = ".rml.json";
    std::vector<std::string> sidecars;
    bool changed = false;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(tools::ToPath(repo), ec)) {
        if (ec || !entry.is_regular_file()) continue;
        const auto name_u8 = entry.path().filename().u8string();
        const std::string name(name_u8.begin(), name_u8.end());
        if (name.size() <= kSuffix.size()) continue;
        if (tools::ToLower(name.substr(name.size() - kSuffix.size())) != kSuffix) continue;

        const std::string key = name.substr(0, name.size() - kSuffix.size());
        if (key.empty()) continue;

        if (index.Get(key).IsNull()) {
            const std::string text = ReadTextFile(tools::JoinPath(repo, name));
            Json::Value value;
            Json parser;
            std::string parse_error;
            if (!text.empty() && parser.Parse(text, value, parse_error)) {
                index[key] = std::move(value);
                changed = true;
            }
        }
        sidecars.push_back(tools::JoinPath(repo, name));
    }

    if (changed) {
        std::string error;
        if (!index.commit(error)) return false;
    }
    for (const auto& path : sidecars) {
        std::error_code remove_ec;
        std::filesystem::remove(tools::ToPath(path), remove_ec);
    }
    return true;
}

bool Directory::ReadRepositoryModText(const std::string& file, std::string& out) const {
    if (!EnsureRepositoryIndex()) return false;

    auto index = JsonArena::Instance().Open(Join(RepositoryModsRoot(), "rml.json"));
    if (!index) return false;
    const Json::Value& value = index.Get(file);
    if (value.IsNull()) return false;
    out = value.Dump();
    return !out.empty();
}

bool Directory::WriteRepositoryModText(const std::string& file, std::string_view text) const {
    if (!EnsureRepositoryIndex()) return false;

    auto index = JsonArena::Instance().Open(Join(RepositoryModsRoot(), "rml.json"));
    if (!index) return false;

    if (text.empty()) {
        index.remove(file);
    } else {
        Json::Value value;
        Json parser;
        std::string parse_error;
        if (!parser.Parse(text, value, parse_error)) return false;
        index[file] = std::move(value);
    }

    std::string error;
    return index.commit(error);
}

bool Directory::ReadDataFile(const std::string& name, std::string& out) {
    const std::string path = DataFile(name);
    if (!tools::PathExists(path)) return false;
    out = tools::ReadTextFile(path);
    return !out.empty();
}

bool Directory::WriteDataFile(const std::string& name, std::string_view text) {
    return WriteTextEnsured(DataFile(name), text);
}

std::string Directory::ReadTextFile(const std::string& path) {
    return tools::ReadTextFile(path);
}

bool Directory::ReadTextFile(const std::string& path, std::string& out) {
    if (!tools::PathExists(path)) return false;
    out = tools::ReadTextFile(path);
    return true;
}

bool Directory::WriteTextFile(const std::string& path, std::string_view text) {
    return WriteTextEnsured(path, text);
}

std::string Directory::VersionsRoot() const { return Join(game_root_, "versions"); }

std::string Directory::VersionDir(const std::string& id) const {
    return Join(VersionsRoot(), id);
}

std::string Directory::VersionJson(const std::string& id) const {
    return Join(VersionDir(id), id + ".json");
}

std::string Directory::VersionJar(const std::string& id) const {
    return Join(VersionDir(id), id + ".jar");
}

std::string Directory::NativesDir(const std::string& id) const {
    return Join(VersionDir(id), id + "-natives");
}

std::string Directory::LibrariesRoot() const { return Join(game_root_, "libraries"); }

std::string Directory::AssetsRoot() const { return Join(game_root_, "assets"); }

std::string Directory::AssetIndexesRoot() const { return Join(AssetsRoot(), "indexes"); }

std::string Directory::AssetObjectsRoot() const { return Join(AssetsRoot(), "objects"); }

std::string Directory::ServersRoot() const { return Join(game_root_, "servers"); }

std::string Directory::ServerDir(const std::string& dir) const {
    return Join(ServersRoot(), dir);
}

std::string Directory::ServerManifest(const std::string& dir) const {
    return Join(ServerDir(dir), "rml.json");
}

std::string Directory::ServerRunStateFile() { return DataFile("server-runstate.json"); }

std::string Directory::GameLogsRoot() const { return Join(game_root_, "logs"); }

std::string Directory::ModsRoot() const { return Join(game_root_, "mods"); }

bool Directory::EnsureModsRoot(std::string& error) const {
    std::error_code ec;
    std::filesystem::create_directories(ToPath(ModsRoot()), ec);
    if (ec) {
        error = "创建 Mods 目录失败: " + ModsRoot();
        return false;
    }
    return true;
}

std::string Directory::InstalledGameRoot(const std::string& id) const {
    // Mod 版使用独立 gameDir，这样 versions/<id>/mods 才会被 Forge/Fabric 加载
    return VersionDir(id);
}

std::string Directory::InstalledModsRoot(const std::string& id) const {
    return Join(InstalledGameRoot(id), "mods");
}

bool Directory::EnsureInstalledModsRoot(const std::string& id, std::string& error) const {
    std::error_code ec;
    std::filesystem::create_directories(ToPath(InstalledModsRoot(id)), ec);
    if (ec) {
        error = "创建版本 Mods 目录失败: " + InstalledModsRoot(id);
        return false;
    }
    return true;
}

std::string Directory::ShaderPacksRoot() const { return Join(game_root_, "shaderpacks"); }

std::string Directory::ResourcePacksRoot() const { return Join(game_root_, "resourcepacks"); }

std::string Directory::DataRoot() {
#ifdef _WIN32
    char buffer[32768];
    const DWORD length = GetEnvironmentVariableA("APPDATA", buffer, sizeof(buffer));
    if (length > 0 && length < sizeof(buffer)) return Join(std::string(buffer, length), "RML");
#else
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') return Join(home, ".rml");
#endif
    return ".";
}

std::string Directory::DataFile(const std::string& name) { return Join(DataRoot(), name); }

std::string Directory::DefaultGameRoot() {
#ifdef _WIN32
    char buffer[32768];
    const DWORD length = GetEnvironmentVariableA("APPDATA", buffer, sizeof(buffer));
    if (length > 0 && length < sizeof(buffer)) {
        return Join(std::string(buffer, length), ".minecraft");
    }
#endif
    return ".minecraft";
}

std::string Directory::SourcesFile() { return DataFile("sources.json"); }

std::string Directory::ServersFile() { return DataFile("servers.json"); }

std::string Directory::CacheRoot() { return DataFile("cache"); }

std::string Directory::InstallerCacheDir() { return Join(CacheRoot(), "installers"); }

std::string Directory::TempRoot() { return DataFile("tmp"); }

std::string Directory::TempFile(const std::string& name) { return Join(TempRoot(), name); }

std::string Directory::RuntimesRoot() { return DataFile("runtimes"); }

std::string Directory::LauncherLogsRoot() { return DataFile("logs"); }

std::string Directory::LauncherLogFile() { return Join(LauncherLogsRoot(), "rml-debug.log"); }
