// 服务端清单：安装来自 versions/ 里 rml.json 标记为 server 的入口；
// 实例唯一事实来源是 servers/<dir>/rml.json（kind=serverInstance），
// %APPDATA%\RML\servers.json 只作为旧数据导入源保留。
#pragma once

#include <string>
#include <vector>

#include "app/server/Instance/Instance.h"

namespace server {

class Catalog {
public:
    struct Installation {
        std::string id;         // 版本目录名
        std::string name;       // 显示名
        std::string loader;     // 原版 / Fabric / Forge
        std::string minecraft;  // MC 版本
        std::string core;       // 共享核心 id
        std::string jar;        // server.jar 绝对路径；Forge 参数文件启动时为空
        std::string arg_file;   // Forge: 相对安装目录的 win_args.txt
        int java_major = 8;
        int refs = 0;           // 引用这个安装的服务端实例数
        // Fabric 服务端：mainClass + 相对 libraries 的 classpath；原版为空
        std::string main_class;
        std::vector<std::string> classpath;
    };

    explicit Catalog(std::string game_dir);

    void Reload();

    const std::vector<Installation>& Installations() const { return installations_; }
    const std::vector<Instance>& Instances() const { return instances_; }
    std::vector<Instance>& Instances() { return instances_; }

    int IndexOf(const std::string& dir) const;
    // 写实例 manifest（不含 pid）；pid 只走 SaveRunState。
    bool Save(std::string& error) const;
    bool SaveInstance(size_t index, std::string& error) const;
    bool SaveRunState(std::string& error) const;

    static std::string ServersDir(const std::string& game_dir);
    static std::string InstallationsDir(const std::string& game_dir);
    static std::string ManifestPath(const std::string& game_dir, const std::string& dir);
    static std::string ConfigPath();
    static std::string RunStatePath();

private:
    void ScanInstallations();
    void LoadInstances();

    std::string game_dir_;
    std::vector<Installation> installations_;
    std::vector<Instance> instances_;
};

}  // namespace server
