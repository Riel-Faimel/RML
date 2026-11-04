#include "app/directory/Directory.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <utility>

#include "app/tools/Json.h"

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

std::string BaseOfLoaderVersion(const std::string& id) {
    for (const char* suffix : {"-forge-", "-neoforge-"}) {
        const size_t pos = id.find(suffix);
        if (pos != std::string::npos && pos > 0) return id.substr(0, pos);
    }
    return {};
}

bool ReadJsonFile(const std::string& path, Json::Value& out) {
    std::ifstream stream(ToPath(path), std::ios::binary);
    if (!stream) return false;
    const std::string text((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    Json json;
    std::string error;
    return !text.empty() && json.Parse(text, out, error);
}

}  // namespace

Directory::Directory(std::string game_root) : game_root_(std::move(game_root)) {}

std::string Directory::Join(const std::string& parent, const std::string& child) {
    return ::Join(parent, child);
}

std::string Directory::GameFile(const std::string& name) const {
    return Join(game_root_, name);
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

void Directory::RefreshVersions() {
    versions_.clear();
    visible_.clear();
    dependency_owners_.clear();

    std::error_code ec;
    std::vector<std::string> ids;
    for (const auto& entry : std::filesystem::directory_iterator(ToPath(VersionsRoot()), ec)) {
        if (ec || !entry.is_directory()) continue;
        const auto name = entry.path().filename().u8string();
        ids.emplace_back(name.begin(), name.end());
    }
    std::sort(ids.begin(), ids.end());

    for (const auto& id : ids) {
        VersionEntry version;
        version.id = id;
        version.name = id;
        version.directory = VersionDir(id);
        version.has_json = std::filesystem::is_regular_file(ToPath(VersionJson(id)));
        if (!version.has_json) continue;

        Json::Value version_root;
        if (!ReadJsonFile(VersionJson(id), version_root)) continue;
        if (version_root["id"].AsString().empty()) continue;
        version.minecraft_version = version_root["inheritsFrom"].AsString();
        if (version.minecraft_version.empty()) version.minecraft_version = id;

        version.has_jar = std::filesystem::is_regular_file(ToPath(VersionJar(id)));

        Json::Value marker;
        version.user_installed = true;
        if (ReadJsonFile(version.directory + "/rml.json", marker)) {
            version.managed = marker["managed"].AsBool(false);
            version.complete = marker["complete"].AsBool(true);
            version.loader = marker["loader"].AsString();
            version.is_server = marker["kind"].AsString() == "server";
            if (marker.Find("user") != nullptr) {
                version.user_installed = marker["user"].AsBool(true);
            }
            const std::string name = marker["name"].AsString();
            if (!name.empty()) version.name = name;
        }

        versions_.push_back(std::move(version));
    }

    // Forge/NeoForge 的底座：默认它只是加载器的内部依赖；
    // 只有底座显式标记为 user 时，才保留成用户可独立启动的版本。
    std::map<std::string, std::string> owners;
    for (const auto& version : versions_) {
        if (!version.managed || !version.complete || version.is_server) continue;
        const std::string base = BaseOfLoaderVersion(version.id);
        if (base.empty()) continue;
        const auto base_it = std::find_if(versions_.begin(), versions_.end(),
                                          [&base](const VersionEntry& item) {
                                              return item.id == base;
                                          });
        if (base_it == versions_.end()) continue;

        auto found = owners.find(base);
        if (found == owners.end() || version.id > found->second) owners[base] = version.id;
    }

    for (auto& version : versions_) {
        const auto owner = owners.find(version.id);
        if (owner != owners.end() && !version.user_installed) {
            version.dependency = true;
            dependency_owners_[version.id] = owner->second;
        } else {
            visible_.push_back(version);
        }
    }
}

std::string Directory::DependencyOwner(const std::string& id) const {
    const auto found = dependency_owners_.find(id);
    return found == dependency_owners_.end() ? std::string() : found->second;
}

bool Directory::IsDependencyVersion(const std::string& id) const {
    return dependency_owners_.find(id) != dependency_owners_.end();
}

std::string Directory::LibrariesRoot() const { return Join(game_root_, "libraries"); }

std::string Directory::AssetsRoot() const { return Join(game_root_, "assets"); }

std::string Directory::AssetIndexesRoot() const { return Join(AssetsRoot(), "indexes"); }

std::string Directory::AssetObjectsRoot() const { return Join(AssetsRoot(), "objects"); }

std::string Directory::ServersRoot() const { return Join(game_root_, "servers"); }

std::string Directory::ServerDir(const std::string& dir) const {
    return Join(ServersRoot(), dir);
}

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

std::string Directory::InstalledManifest(const std::string& id) const {
    return Join(InstalledGameRoot(id), "mods.json");
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

std::string Directory::DatapacksRoot() const { return Join(game_root_, "datapacks"); }

std::string Directory::SavesRoot() const { return Join(game_root_, "saves"); }

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

std::string Directory::SettingsFile() { return DataFile("settings.json"); }

std::string Directory::SourcesFile() { return DataFile("sources.json"); }

std::string Directory::ServersFile() { return DataFile("servers.json"); }

std::string Directory::CacheRoot() { return DataFile("cache"); }

std::string Directory::InstallerCacheDir() { return Join(CacheRoot(), "installers"); }

std::string Directory::TempRoot() { return DataFile("tmp"); }

std::string Directory::TempFile(const std::string& name) { return Join(TempRoot(), name); }

std::string Directory::RuntimesRoot() { return DataFile("runtimes"); }

std::string Directory::LauncherLogsRoot() { return DataFile("logs"); }

std::string Directory::LauncherLogFile() { return Join(LauncherLogsRoot(), "rml-debug.log"); }
