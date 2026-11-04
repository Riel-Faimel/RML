#include "app/server/Server.h"

#include <filesystem>
#include <system_error>

#include "app/directory/Directory.h"
#include "app/tools/tools.h"

bool Server::RemoveInstallation(int index, std::string& error) {
    const auto& installations = catalog_.Installations();
    if (index < 0 || static_cast<size_t>(index) >= installations.size()) {
        error = "服务端安装不存在";
        return false;
    }
    const std::string id = installations[static_cast<size_t>(index)].id;
    for (const auto& instance : catalog_.Instances()) {
        if (instance.installation != id) continue;
        error = "仍有服务器实例使用这个服务端安装：" + instance.name;
        if (Running(catalog_.IndexOf(instance.dir))) error += "（正在运行，请先停止）";
        return false;
    }

    const std::string dir = Directory(game_dir_).VersionDir(id);
    if (!tools::IsDirectory(dir)) {
        error = "服务端安装目录不存在: " + dir;
        return false;
    }

    // 只删安装入口；共享核心由 GC 显式清理，绝不连带删除。
    std::error_code ec;
    std::filesystem::remove_all(tools::ToPath(dir), ec);
    if (ec) {
        error = "删除服务端安装失败: " + dir + "（" + ec.message() + "）";
        return false;
    }
    catalog_.Reload();
    return true;
}

bool Server::AnyRunning() const {
    for (size_t i = 0; i < catalog_.Instances().size(); ++i) {
        if (Running(static_cast<int>(i))) return true;
    }
    return false;
}
