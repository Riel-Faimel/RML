// 服务端清单：扫描 versions/ 里 rml.json 标记为 server 的安装，
// 以及 %APPDATA%\RML\servers.json 里的运行实例。
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
        std::string loader;     // 原版 / Fabric
        std::string minecraft;  // MC 版本
        std::string jar;        // server.jar 绝对路径
        int java_major = 8;
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
    bool Save(std::string& error) const;

    static std::string ServersDir(const std::string& game_dir);
    static std::string InstallationsDir(const std::string& game_dir);
    static std::string ConfigPath();

private:
    void ScanInstallations();
    void LoadInstances();

    std::string game_dir_;
    std::vector<Installation> installations_;
    std::vector<Instance> instances_;
};

}  // namespace server
