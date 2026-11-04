// 服务端模块：安装清单 + 运行实例 + 进程控制。
// 模块级类放全局命名空间；组件（Catalog/Runner/Installer/Instance）在 server 命名空间。
#pragma once

#include <string>
#include <vector>

#include "app/launch/Environment/Environment.h"
#include "app/server/Catalog/Catalog.h"
#include "app/server/Runtime/Control.h"

class Server {
public:
    struct CreateOptions {
        std::string name;
        std::string installation;  // Catalog::Installation::id
        int port = 25565;
        int memory_mb = 2048;
        std::string gamemode = "survival";
        std::string difficulty = "easy";
        int max_players = 20;
        std::string seed;
        bool online_mode = false;
    };

    explicit Server(std::string game_dir);

    void Refresh();

    const std::string& GameDir() const { return game_dir_; }
    const std::vector<server::Catalog::Installation>& Installations() const;
    const std::vector<server::Instance>& Instances() const;

    bool Create(const CreateOptions& options, std::string& error);
    bool Remove(int index, std::string& error);
    // 删除服务端安装（versions/<id>），不是服务器实例；仍有实例引用时拒绝。
    bool RemoveInstallation(int index, std::string& error);
    // 迁移/删除前检查：有没有服务端实例正在运行。
    bool AnyRunning() const;
    bool Rename(int index, const std::string& name, std::string& error);
    bool Update(int index, int port, int memory_mb, const std::string& gamemode,
                const std::string& difficulty, int max_players, std::string& error);

    // server.properties 全部条目（详细编辑用）
    bool ReadProperties(int index, std::vector<std::pair<std::string, std::string>>& out,
                        std::string& error) const;
    bool SaveProperties(int index,
                        const std::vector<std::pair<std::string, std::string>>& entries,
                        std::string& error);

    bool Start(int index, std::string& error);
    bool Stop(int index, bool force, std::string& error);
    bool Send(int index, const std::string& command, std::string& error);

    bool Running(int index) const;
    int Pid(int index) const;
    std::string Status(int index) const;
    std::string Console(int index, size_t max_bytes) const;
    // 清理已退出进程的 pid，并把变化写进独立 run-state 缓存。
    void SyncPids();

private:
    bool EnsureFile(const server::Instance& instance, std::string& error);
    std::string JavaFor(const server::Instance& instance, std::string& error) const;

    std::string game_dir_;
    launch::Environment environment_;
    server::Catalog catalog_;
    server::runtime::Control runtime_;
};
