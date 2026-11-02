// 加载器安装的公共流程：安装器（installer.jar）→ install_profile.json →
// 版本描述 / 依赖库 / 处理器 → 全量补全。Forge 和 NeoForge 只差「怎么找安装器」。
#pragma once

#include <string>

#include "app/download/Flow/Flow.h"

namespace download {
namespace flow {

class Profile {
public:
    static bool Install(const Context& context, const Request& request, const std::string& loader,
                        const std::string& maven_version, const std::string& installer_url,
                        const Progress& progress, Outcome& outcome, std::string& error);
};

}  // namespace flow
}  // namespace download
