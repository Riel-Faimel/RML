// 安装器的处理器：Forge / NeoForge 的 install_profile.json 里有一串 java 工具，
// 按 data → processors 跑完，产物是补丁 jar。这里只负责把清单翻译成命令，不掺 Qt。
#pragma once

#include <string>

#include "app/directory/Flow/Flow.h"
#include "app/tools/Json.h"

namespace directory {
namespace flow {

class Processors {
public:
    explicit Processors(installer::Transfer& transfer) : transfer_(transfer) {}

    // install_profile.libraries：处理器和 universal jar 的依赖
    bool DownloadLibraries(const std::string& libraries_root, const Json::Value& profile,
                           const Progress& progress, std::string& error) const;

    // data（文件/工件/字面量）→ processors（sides 只跑 client）→ 校验 outputs
    bool Run(const std::string& java, const std::string& game_dir, const std::string& installer_jar,
             const std::string& mc_version, const std::string& work_dir, const Json::Value& profile,
             const Progress& progress, std::string& error) const;

private:
    installer::Transfer& transfer_;
};

}  // namespace flow
}  // namespace directory
