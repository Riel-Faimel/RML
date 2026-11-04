#include "app/directory/Flow/Fabric.h"

#include "app/directory/Directory.h"
#include "app/directory/Flow/Support.h"
#include "app/directory/Flow/Transaction.h"
#include "app/directory/Installer/Version/Version.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace directory {
namespace flow {

bool Fabric::RunImpl(const Context& context, const Request& request, const Progress& progress,
                     Outcome& outcome, std::string& error) {
    const std::string& mc_version = request.mc_version;
    const Directory paths(request.game_dir);
    const bool base_preexisting =
        tools::PathExists(paths.VersionJson(mc_version)) ||
        tools::PathExists(paths.CoreJson(paths.CoreForEntry(mc_version)));

    Support::Report(progress, "准备原版 " + mc_version, 4);
    if (!base_preexisting) {
        if (!Support::WriteBaseMarker(request.game_dir, mc_version, error)) return false;
    }
    if (!Support::EnsureVanillaJson(context, mc_version, error)) return false;

    Support::Report(progress, "获取 Fabric 加载器", 12);
    std::string loader_version = request.loader_version;
    if (loader_version.empty() &&
        !context.sources.Loaders().FabricLatest(mc_version, loader_version, error)) {
        return false;
    }

    std::string version_json;
    if (!context.sources.Loaders().FabricProfile(mc_version, loader_version, version_json, error)) {
        return false;
    }

    std::string version_id;
    Json::Value root;
    Json json;
    std::string parse_error;
    if (json.Parse(version_json, root, parse_error) && !root["id"].AsString().empty()) {
        version_id = root["id"].AsString();
    }
    if (version_id.empty()) version_id = "fabric-loader-" + loader_version + "-" + mc_version;

    Support::Report(progress, "写入版本描述", 18);
    Transaction txn(request.game_dir, version_id, "loader", "Fabric", loader_version,
                    mc_version, mc_version, false);
    if (!txn.Ok()) {
        error = txn.Error();
        return false;
    }
    if (!installer::Version::WriteVersionJson(request.game_dir, version_id, version_json, error)) {
        return false;
    }

    context.versions.Refresh();
    if (!context.versions.Select(version_id)) {
        error = "装完扫描不到版本: " + version_id;
        return false;
    }

    installer::Transfer::Result result;
    if (!context.installer.Versions().Install(context.installer.Transfers(),
                                              Support::Segment(progress, 20, 100), result, error,
                                              true, context.sources.Origins().AssetsRoot())) {
        error = "Fabric 安装未完成：" + error;
        return false;
    }

    if (!txn.Commit()) {
        error = txn.Error();
        return false;
    }

    outcome.version_id = version_id;
    outcome.loader_version = loader_version;
    return true;
}

}  // namespace flow
}  // namespace directory
