#include "app/directory/Directory.h"

#include <cstdlib>
#include <utility>

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
