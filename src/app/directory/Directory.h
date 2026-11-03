// 目录管理模块：项目里所有“数据目录 / 游戏目录”的拼接只从这里出。
// 其他模块依赖本模块，不再各自拼 versions/libraries/servers 等路径。
#pragma once

#include <string>

class Directory {
public:
    explicit Directory(std::string game_root);

    const std::string& GameRoot() const { return game_root_; }
    std::string GameFile(const std::string& name) const;

    std::string VersionsRoot() const;
    std::string VersionDir(const std::string& id) const;
    std::string VersionJson(const std::string& id) const;
    std::string VersionJar(const std::string& id) const;
    std::string NativesDir(const std::string& id) const;

    std::string LibrariesRoot() const;
    std::string AssetsRoot() const;
    std::string AssetIndexesRoot() const;
    std::string AssetObjectsRoot() const;

    std::string ServersRoot() const;
    std::string ServerDir(const std::string& dir) const;
    std::string GameLogsRoot() const;

    std::string ModsRoot() const;
    std::string ShaderPacksRoot() const;
    std::string ResourcePacksRoot() const;
    std::string DatapacksRoot() const;
    std::string SavesRoot() const;

    // —— RML 自己的数据目录（%APPDATA%\RML） ——
    static std::string DataRoot();
    static std::string DefaultGameRoot();
    static std::string DataFile(const std::string& name);
    static std::string SettingsFile();
    static std::string SourcesFile();
    static std::string ServersFile();
    static std::string CacheRoot();
    static std::string InstallerCacheDir();
    static std::string TempRoot();
    static std::string TempFile(const std::string& name);
    static std::string RuntimesRoot();
    static std::string LauncherLogsRoot();
    static std::string LauncherLogFile();

private:
    static std::string Join(const std::string& parent, const std::string& child);

    std::string game_root_;
};
