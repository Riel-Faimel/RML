// 安装策略：编译期多态（CRTP）。新增一个加载器 = 新增一个 Flow 子类，不改调用方。
//
// 每个策略自己负责「拿版本描述 → 落盘 → 下载 → 必要的后处理」，公共部分放在 Support /
// Processors / Profile 里；Flow 本身只管分发。
#pragma once

#include <functional>
#include <string>

#include "app/directory/Installer/Installer.h"
#include "app/download/VersionSource/VersionSource.h"
#include "app/launch/Environment/Environment.h"
#include "app/launch/Version/Version.h"

namespace directory {
namespace flow {

using Progress = installer::Transfer::Progress;

struct Request {
    std::string game_dir;
    std::string mc_version;  // Mojang 的版本 id
    std::string loader;      // 原版 / Fabric / Forge / NeoForge
    std::string loader_version;  // 用户选定的加载器版本；空表示取 latest
};

struct Outcome {
    std::string version_id;      // 装出来的版本目录名
    std::string loader_version;  // Fabric / Forge / NeoForge 的版本号
};

// 策略运行时能用到的东西：版本选择、Java 环境、远端清单、传输与落盘
struct Context {
    launch::Version& versions;
    launch::Environment& environment;
    download::VersionSource& sources;
    directory::Installer& installer;
};

template <typename Derived>
class Flow {
public:
    bool Run(const Context& context, const Request& request, const Progress& progress,
             Outcome& outcome, std::string& error) {
        return static_cast<Derived*>(this)->RunImpl(context, request, progress, outcome, error);
    }
};

}  // namespace flow
}  // namespace directory
