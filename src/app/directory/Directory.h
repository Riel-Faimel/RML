// 目录管理模块：项目里所有“数据目录 / 游戏目录”的拼接和版本目录容器只从这里出。
// 其他模块持有这里返回的容器引用，不复制、不自己拼路径。
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class Directory;

namespace directory {
namespace flow {
struct Context;
struct Request;
struct Outcome;
}  // namespace flow
}  // namespace directory

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
        bool valid = true;
        std::string role;  // rml.json: game / loader / base / server
        std::string state; // rml.json: installing / committed / failed
        std::string base;  // loader 版本依赖的原版 id（父核心入口）
        std::string core;  // 共享游戏核心 id（cores/<id>）
        bool has_core = false;
        bool fresh = false;
        std::string loader;
        std::string minecraft_version;  // 继承链里的原版版本，内容筛选用
        bool is_server = false;
        bool user_installed = false;
        bool dependency = false;  // Forge/NeoForge 的原版底座，不对外显示
    };

    // 安装器只持有实际用到的几个根，不再自己拼 .minecraft 内部路径。
    struct InstallTarget {
        std::string client_jar;
        std::string libraries_root;
        std::string asset_indexes_root;
        std::string asset_objects_root;

        std::string Library(const std::string& relative) const;
        std::string AssetObject(const std::string& hash) const;
        std::string AssetIndex(const std::string& id) const;
    };

    // 安装入口：按版本 id 给出目录布局。
    InstallTarget Install(const std::string& version_id) const;

    // 安装执行入口：调用者选安装器类型 ID，Directory 调用对应 Flow。
    bool Install(const std::string& installer_type_id,
                 const directory::flow::Context& context,
                 const directory::flow::Request& request,
                 const std::function<void(const std::string&, int, int)>& progress,
                 directory::flow::Outcome& outcome, std::string& error) const;

    explicit Directory(std::string game_root);

    const std::string& GameRoot() const { return game_root_; }
    std::string GameFile(const std::string& name) const;

    // .minecraft 根目录和版本目录的数据读写入口。
    bool ReadVersionText(const std::string& id, const std::string& relative,
                         std::string& out) const;
    bool WriteVersionText(const std::string& id, const std::string& relative,
                          std::string_view text) const;
    bool ReadRepositoryModText(const std::string& file, std::string& out) const;
    bool WriteRepositoryModText(const std::string& file, std::string_view text) const;

    // %APPDATA%\RML 数据文件。
    static bool ReadDataFile(const std::string& name, std::string& out);
    static bool WriteDataFile(const std::string& name, std::string_view text);

    // 底层文本读写也由 Directory 收口；其他模块不再直接碰文件。
    static std::string ReadTextFile(const std::string& path);
    static bool ReadTextFile(const std::string& path, std::string& out);
    static bool WriteTextFile(const std::string& path, std::string_view text);

    std::string VersionsRoot() const;
    std::string VersionDir(const std::string& id) const;
    std::string VersionJson(const std::string& id) const;
    std::string VersionJar(const std::string& id) const;
    std::string NativesDir(const std::string& id) const;

    // 共享游戏核心：cores/<id>，用户不可直接访问。
    std::string CoresRoot() const;
    std::string CoreDir(const std::string& id) const;
    std::string CoreJson(const std::string& id) const;
    std::string CoreJar(const std::string& id) const;
    std::string CoreNatives(const std::string& id) const;
    bool EnsureCore(const std::string& core_id, const std::string& source_version,
                    std::string& error) const;

    // 核心清单：refs = 引用它的 versions 入口（含服务端安装），不包含实例。
    struct CoreEntry {
        std::string id;
        std::string directory;
        int refs = 0;
        int64_t bytes = 0;
        bool valid = true;  // 有 kind=core 的 rml.json；未知目录不参与清理
        bool Orphan() const { return valid && refs == 0; }
    };

    // 核心与入口分离迁移：dry-run 只出计划；正式迁移带 WAL、rml 备份与回滚。
    struct MigrationReport {
        bool ok = false;
        bool dry_run = true;
        int moved = 0;
        int linked = 0;
        int dropped = 0;
        int markers = 0;
        int skipped = 0;
        int cores_created = 0;
        std::string journal;
        std::string error;
        std::vector<std::string> lines;
        bool Empty() const { return lines.empty(); }
    };
    bool MigrateCores(bool dry_run, MigrationReport& report,
                      const std::string& only_entry = std::string()) const;
    bool RollbackLastMigration(std::string& error, std::string& report) const;

    // 扫描 versions/，重建所有版本容器；调用方持有引用即可看到新内容。
    // 版本扫描与 rml.json 事务恢复。
    void RefreshVersions();
    void RefreshCores();
    void RecoverTransactions();
    const std::vector<VersionEntry>& Versions() const { return versions_; }
    const std::vector<VersionEntry>& VisibleVersions() const { return visible_; }
    const std::vector<CoreEntry>& Cores() const { return cores_; }
    const CoreEntry* FindCore(const std::string& id) const;
    // 入口实际引用的核心 id；没有显式 core 时退回入口 id。
    std::string CoreForEntry(const std::string& id) const;
    // 核心 id 只能是单层目录名，防止 rml 里的异常值把路径带出 cores。
    static bool ValidCoreId(const std::string& id);
    // 只删除没有引用的核心；有引用时返回中文错误。
    bool RemoveCore(const std::string& id, std::string& error) const;
    int RemoveOrphanCores(std::string& error) const;

    // 加载器版本依赖的原版底座（例如 1.16.5-forge-... 依赖 1.16.5）。
    // 返回空表示 id 不是被隐藏的底座；否则返回应该取而代之的加载器版本 id。
    std::string DependencyOwner(const std::string& id) const;

    std::string LibrariesRoot() const;
    std::string AssetsRoot() const;
    std::string AssetIndexesRoot() const;
    std::string AssetObjectsRoot() const;

    std::string ServersRoot() const;
    std::string ServerDir(const std::string& dir) const;
    std::string ServerManifest(const std::string& dir) const;
    static std::string ServerRunStateFile();
    std::string GameLogsRoot() const;

    std::string ModsRoot() const;                 // 全局 Mod 仓库
    std::string RepositoryModsRoot() const { return ModsRoot(); }
    bool EnsureModsRoot(std::string& error) const;
    std::string InstalledGameRoot(const std::string& id) const;
    std::string InstalledModsRoot(const std::string& id) const;
    bool EnsureInstalledModsRoot(const std::string& id, std::string& error) const;
    std::string ShaderPacksRoot() const;
    std::string ResourcePacksRoot() const;

    // —— RML 自己的数据目录（%APPDATA%\RML） ——
    static std::string DataRoot();
    static std::string DefaultGameRoot();
    static std::string DataFile(const std::string& name);
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
    // 仓库里所有模组共用一个 mods/rml.json；旧 *.jar.rml.json 会在这里合并后删除。
    bool EnsureRepositoryIndex() const;

    std::string game_root_;
    std::vector<VersionEntry> versions_;
    std::vector<VersionEntry> visible_;
    std::vector<CoreEntry> cores_;
    std::map<std::string, std::string> dependency_owners_;
};
