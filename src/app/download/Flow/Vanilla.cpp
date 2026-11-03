#include "app/download/Flow/Vanilla.h"

#include "app/download/Flow/Support.h"
#include "app/download/Installer/Version/Version.h"
#include "app/tools/tools.h"

namespace download {
namespace flow {

bool Vanilla::RunImpl(const Context& context, const Request& request, const Progress& progress,
                      Outcome& outcome, std::string& error) {
    Support::Report(progress, "读取原版 " + request.mc_version + " 的版本描述", 5);
    if (!Support::EnsureVanillaJson(context, request.mc_version, error)) return false;

    if (!installer::Version::WriteMarker(request.game_dir, request.mc_version, "原版", false)) {
        error = "写托管标记失败: " + request.mc_version;
        return false;
    }

    context.versions.Refresh();
    if (!context.versions.Select(request.mc_version)) {
        error = "装完扫描不到版本: " + request.mc_version;
        return false;
    }

    installer::Transfer::Result result;
    if (!context.installer.Versions().Install(context.installer.Transfers(),
                                              Support::Segment(progress, 10, 100), result, error,
                                              true, context.sources.Origins().AssetsRoot())) {
        error = "原版安装未完成：" + error;
        return false;
    }

    installer::Version::WriteMarker(request.game_dir, request.mc_version, "原版", true, true, true);
    outcome.version_id = request.mc_version;
    return true;
}

}  // namespace flow
}  // namespace download
