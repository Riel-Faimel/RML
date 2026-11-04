#include "app/server/Server.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "app/directory/Directory.h"
#include "app/server/Naming/Name.h"
#include "app/server/Properties/Properties.h"
#include "app/tools/tools.h"
namespace {

std::string TrimWhitespace(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) --end;
    return text.substr(begin, end - begin);
}

std::string InstanceDir(const std::string& game_dir, const server::Instance& instance) {
    return Directory(game_dir).ServerDir(instance.dir);
}

}  // namespace

Server::Server(std::string game_dir)
    : game_dir_(std::move(game_dir)), environment_(), catalog_(game_dir_) {
    Refresh();
}

void Server::Refresh() {
    catalog_.Reload();
    SyncPids();
}

void Server::SyncPids() {
    if (!runtime_.Sync(catalog_.Instances())) return;

    // pid 只写独立运行状态缓存，不碰实例 manifest。
    std::string ignored;
    catalog_.SaveRunState(ignored);
}

const std::vector<server::Catalog::Installation>& Server::Installations() const {
    return catalog_.Installations();
}

const std::vector<server::Instance>& Server::Instances() const { return catalog_.Instances(); }

// 成员函数 Create 会遮蔽同名嵌套类型，参数类型用完整限定名
bool Server::Create(const Server::CreateOptions& options, std::string& error) {
    const auto& installations = catalog_.Installations();
    const auto installation =
        std::find_if(installations.begin(), installations.end(),
                     [&options](const server::Catalog::Installation& entry) {
                         return entry.id == options.installation;
                     });
    if (installation == installations.end()) {
        error = "找不到服务端安装: " + options.installation;
        return false;
    }

    const std::string name = TrimWhitespace(options.name);
    if (name.empty()) {
        error = "服务器名称不能为空";
        return false;
    }

    const std::string dir = server::naming::Name::Unique(name, game_dir_, std::string(), catalog_);
    const std::string servers = server::Catalog::ServersDir(game_dir_);
    const std::string instance_dir = tools::JoinPath(servers, dir);
    std::string dir_error;
    if (!tools::EnsureDir(instance_dir, dir_error)) {
        error = "创建服务器目录失败: " + instance_dir + "（" + dir_error + "）";
        return false;
    }

    server::Instance instance;
    instance.name = name;
    instance.dir = dir;
    instance.installation = options.installation;
    instance.port = options.port;
    instance.memory_mb = options.memory_mb;
    instance.gamemode = options.gamemode;
    instance.difficulty = options.difficulty;
    instance.max_players = options.max_players;
    instance.online_mode = options.online_mode;
    instance.seed = options.seed;

    if (!EnsureFile(instance, error)) return false;

    catalog_.Instances().push_back(std::move(instance));
    if (!catalog_.SaveInstance(catalog_.Instances().size() - 1, error)) {
        error = "保存服务器实例失败: " + error;
        return false;
    }
    return true;
}

bool Server::Remove(int index, std::string& error) {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) {
        error = "服务器实例不存在";
        return false;
    }
    if (Running(index)) {
        error = "服务器正在运行，请先停止: " + catalog_.Instances()[index].name;
        return false;
    }

    const server::Instance instance = catalog_.Instances()[index];
    const std::string dir = InstanceDir(game_dir_, instance);

    std::error_code ec;
    std::filesystem::remove_all(tools::ToPath(dir), ec);
    if (ec) {
        error = "删除服务器目录失败: " + dir + "（" + ec.message() + "）";
        return false;
    }

    runtime_.Forget(instance.dir);
    catalog_.Instances().erase(catalog_.Instances().begin() + index);

    if (!catalog_.SaveRunState(error)) {
        error = "保存服务器实例状态失败: " + error;
        return false;
    }
    return true;
}

bool Server::Rename(int index, const std::string& name, std::string& error) {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) {
        error = "服务器实例不存在";
        return false;
    }
    if (Running(index)) {
        error = "服务器正在运行，请先停止: " + catalog_.Instances()[index].name;
        return false;
    }

    const std::string new_name = TrimWhitespace(name);
    if (new_name.empty()) {
        error = "服务器名称不能为空";
        return false;
    }

    const server::Instance before = catalog_.Instances()[index];
    const std::string old_dir = before.dir;
    const std::string servers = server::Catalog::ServersDir(game_dir_);
    const std::string new_dir = server::naming::Name::Unique(new_name, game_dir_, old_dir, catalog_);

    if (new_dir != old_dir) {
        const std::string old_path = tools::JoinPath(servers, old_dir);
        const std::string new_path = tools::JoinPath(servers, new_dir);
        std::error_code ec;
        std::filesystem::rename(tools::ToPath(old_path), tools::ToPath(new_path), ec);
        if (ec) {
            error = "重命名服务器目录失败: " + old_path + " -> " + new_path + "（" + ec.message() +
                    "）";
            return false;
        }

        runtime_.Renamed(old_dir, new_dir);
    }

    auto& instance = catalog_.Instances()[index];
    instance.name = new_name;
    instance.dir = new_dir;

    if (!catalog_.SaveInstance(static_cast<size_t>(index), error)) {
        error = "保存服务器实例失败: " + error;
        return false;
    }
    if (!catalog_.SaveRunState(error)) {
        error = "保存服务器实例状态失败: " + error;
        return false;
    }
    return true;
}

bool Server::Update(int index, int port, int memory_mb, const std::string& gamemode,
                    const std::string& difficulty, int max_players, std::string& error) {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) {
        error = "服务器实例不存在";
        return false;
    }
    if (port < 1 || port > 65535) {
        error = "端口必须在 1-65535 之间: " + std::to_string(port);
        return false;
    }
    if (memory_mb < 512) {
        error = "内存至少为 512 MB: " + std::to_string(memory_mb);
        return false;
    }
    server::Instance updated = catalog_.Instances()[index];
    updated.port = port;
    updated.memory_mb = memory_mb;
    updated.gamemode = gamemode;
    updated.difficulty = difficulty;
    updated.max_players = max_players;
    if (!server::properties::File::Write(game_dir_, updated, true, error)) return false;

    catalog_.Instances()[index] = std::move(updated);
    if (!catalog_.SaveInstance(static_cast<size_t>(index), error)) {
        error = "保存服务器实例失败: " + error;
        return false;
    }
    return true;
}

bool Server::ReadProperties(int index, std::vector<std::pair<std::string, std::string>>& out,
                            std::string& error) const {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) {
        error = "服务器实例不存在";
        return false;
    }
    return server::properties::File::Read(game_dir_, catalog_.Instances()[index], out, error);
}

bool Server::SaveProperties(int index,
                            const std::vector<std::pair<std::string, std::string>>& entries,
                            std::string& error) {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) {
        error = "服务器实例不存在";
        return false;
    }

    server::Instance updated;
    if (!server::properties::File::Apply(game_dir_, catalog_.Instances()[index], entries, updated,
                                         error)) {
        return false;
    }

    catalog_.Instances()[index] = std::move(updated);
    if (!catalog_.SaveInstance(static_cast<size_t>(index), error)) {
        error = "保存服务器实例失败: " + error;
        return false;
    }
    return true;
}

bool Server::Start(int index, std::string& error) {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) {
        error = "服务器实例不存在";
        return false;
    }
    if (Running(index)) {
        error = "服务器已在运行: " + catalog_.Instances()[index].name;
        return false;
    }

    const server::Instance& instance = catalog_.Instances()[index];
    const auto& installations = catalog_.Installations();
    const auto installation =
        std::find_if(installations.begin(), installations.end(),
                     [&instance](const server::Catalog::Installation& entry) {
                         return entry.id == instance.installation;
                     });
    if (installation == installations.end()) {
        error = "找不到服务端安装: " + instance.installation + "（实例: " + instance.name + "）";
        return false;
    }
    if (installation->jar.empty() && installation->arg_file.empty()) {
        error = "服务端安装没有可用的启动文件: " + installation->id;
        return false;
    }
    if (!installation->jar.empty() && !tools::PathExists(installation->jar)) {
        error = "服务端 jar 不存在: " + installation->jar;
        return false;
    }
    if (!installation->arg_file.empty() &&
        !tools::PathExists(tools::JoinPath(Directory(game_dir_).VersionDir(installation->id),
                                           installation->arg_file))) {
        error = "服务端参数文件不存在: " + installation->arg_file;
        return false;
    }

    const std::string java = JavaFor(instance, error);
    if (java.empty()) return false;

    if (!EnsureFile(instance, error)) return false;

    if (!runtime_.Start(instance, *installation, java, game_dir_, error)) return false;

    catalog_.Instances()[index].pid = runtime_.Pid(catalog_.Instances()[index]);
    if (!catalog_.SaveRunState(error)) {
        error = "服务器已启动，但保存实例状态失败: " + error;
        return false;
    }
    return true;
}

bool Server::Stop(int index, bool force, std::string& error) {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) {
        error = "服务器实例不存在";
        return false;
    }
    if (!Running(index)) {
        error = "服务器没有运行";
        return false;
    }

    if (!runtime_.Stop(catalog_.Instances()[index], force, error)) return false;

    // 强制停止后进程号立刻失效；优雅停止要等进程自己退出，pid 由 SyncPids 清理
    if (force) {
        catalog_.Instances()[index].pid = 0;
        if (!catalog_.SaveRunState(error)) {
            error = "服务器已停止，但保存实例状态失败: " + error;
            return false;
        }
    }
    return true;
}

bool Server::Send(int index, const std::string& command, std::string& error) {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) {
        error = "服务器实例不存在";
        return false;
    }
    return runtime_.Send(catalog_.Instances()[index], command, error);
}

bool Server::Running(int index) const {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) return false;
    return runtime_.Running(catalog_.Instances()[index]);
}

int Server::Pid(int index) const {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) return 0;
    return runtime_.Pid(catalog_.Instances()[index]);
}

std::string Server::Status(int index) const {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) return "实例不存在";
    return runtime_.Status(catalog_.Instances()[index]);
}

std::string Server::Console(int index, size_t max_bytes) const {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) return {};
    return runtime_.Console(InstanceDir(game_dir_, catalog_.Instances()[index]), max_bytes);
}

bool Server::EnsureFile(const server::Instance& instance, std::string& error) {
    return server::properties::File::Ensure(game_dir_, instance, error);
}

std::string Server::JavaFor(const server::Instance& instance, std::string& error) const {
    const auto& installations = catalog_.Installations();
    const auto installation =
        std::find_if(installations.begin(), installations.end(),
                     [&instance](const server::Catalog::Installation& entry) {
                         return entry.id == instance.installation;
                     });
    if (installation == installations.end()) {
        error = "找不到服务端安装: " + instance.installation + "（实例: " + instance.name + "）";
        return {};
    }

    const int required = installation->java_major > 0 ? installation->java_major : 8;
    const launch::Environment::Java* java = environment_.Pick(required);
    if (java == nullptr) {
        error = "需要 Java " + std::to_string(required) + "，但本机没有找到可用的 Java（实例: " +
                instance.name + "，目录: " + InstanceDir(game_dir_, instance) + "）";
        return {};
    }
    if (java->major < required) {
        error = "需要 Java " + std::to_string(required) + "，当前找到的 Java 是 " +
                std::to_string(java->major) + "（" + java->path + "）";
        return {};
    }
    return java->path;
}
