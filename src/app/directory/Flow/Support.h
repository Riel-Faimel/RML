// 安装策略的公共动作：路径换算、占位符替换、原版底座、安装器解包、manifest 读取。
// 都是无状态的类方法，策略之间共用。
#pragma once

#include <map>
#include <string>

#include "app/directory/Flow/Flow.h"

namespace directory {
namespace flow {

class Support {
public:
    // maven 坐标（group:artifact:version[:classifier][@ext]）→ libraries 下的相对路径
    static std::string MavenPath(const std::string& coordinate);
    static std::string ResolveArtifact(const std::string& libraries_root,
                                       const std::string& coordinate);

    // Forge 安装器里的 {DATA} / 'literal' 展开规则
    static bool ReplaceTokens(const std::map<std::string, std::string>& tokens,
                              const std::string& value, std::string& out, std::string& error);

    // 远端 .sha1；拿不到返回空（调用方退化成不校验）
    static std::string RemoteSha1(const std::string& url);

    // 下载到指定路径；sha1 为空表示不校验。已有且校验通过会跳过。
    static bool DownloadFile(const std::string& url, const std::string& dest, const std::string& sha1,
                             std::string& error);

    // 确保 versions/<mc>/<mc>.json 存在（从 Mojang 清单拿）
    static bool EnsureVanillaJson(const Context& context, const std::string& mc_version,
                                  std::string& error);

    // 安装器顺手创建的原版底座：显式写 role=base，不覆盖用户已有版本。
    static bool WriteBaseMarker(const std::string& game_dir, const std::string& mc_version,
                                std::string& error);

    // 确保 versions/<mc>/<mc>.jar 存在（Forge 的处理器要拿它当输入）
    static bool EnsureClientJar(const Context& context, const std::string& mc_version,
                                const Progress& progress, std::string& error);

    // 处理器 jar 的 Main-Class（处理器清单里没有这个字段，只能读 manifest）
    static bool ReadMainClass(const std::string& jar, const std::string& work_dir,
                              std::string& main_class, std::string& error);

    static std::string FileName(const std::string& path);

    // 进度统一按百分比报：0-100
    static void Report(const Progress& progress, const std::string& label, int percent);
    static Progress Segment(const Progress& progress, int from, int to);
};

}  // namespace flow
}  // namespace directory
