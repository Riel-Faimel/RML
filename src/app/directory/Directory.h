// 目录管理模块：项目里所有“数据目录 / 游戏目录”的拼接和版本目录容器只从这里出。
// 其他模块持有这里返回的容器引用，不复制、不自己拼路径。
#pragma once

#include <map>
#include <string>
#include <vector>

class Directory {
public:
    struct VersionEntry {
        std::string id;
        std::string name;
        std::string directory;
        bool has_json = false;
        bool has_jar = false;
        bool managed = false;
        bool complete = true;
        std::string loader;
        std::string minecraft_version;  // 继承链里的原版版本，内容筛选用
        bool is_server = false;
        bool user_installed = false;
        bool dependency = false;  // Forge/NeoForge 的原版底座，不对外显示
    };

    explicit Directory(std::string game_root);

    const std::string& GameRoot() const { return game_root_; }
    std::string GameFile(const std::string& name) const;

    std::string VersionsRoot() const;
    std::string VersionDir(const std::string& id) const;
    std::string VersionJson(const std::string& id) const;
    std::string VersionJar(const std::string& id) const;
    std::string NativesDir(const std::string& id) const;

    // 扫描 versions/，重建所有版本容器；调用方持有引用即可看到新内容。
    void RefreshVersions();
    const std::vector<VersionEntry>& Versions() const { return versions_; }
    const std::vector<VersionEntry>& VisibleVersions() const { return visible_; }

    // 加载器版本依赖的原版底座（例如 1.16.5-forge-... 依赖 1.16.5）。
    // 返回空表示 id 不是被隐藏的底座；否则返回应该取而代之的加载器版本 id。
    std::string DependencyOwner(const std::string& id) const;
    bool IsDependencyVersion(const std::string& id) const;

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
    std::vector<VersionEntry> versions_;
    std::vector<VersionEntry> visible_;
    std::map<std::string, std::string> dependency_owners_;
};
