// 服务端模块：安装清单 + 运行实例 + 进程控制。
// 模块级类放全局命名空间；组件（Catalog/Runner/Installer/Instance）在 server 命名空间。
#pragma once

#include <map>
#include <string>
#include <vector>

#include "app/launch/Environment/Environment.h"
#include "app/server/Catalog/Catalog.h"
#include "app/server/Runner/Runner.h"

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
        bool online_mode = false;
    };

    explicit Server(std::string game_dir);

    void Refresh();

    const std::string& GameDir() const { return game_dir_; }
    const std::vector<server::Catalog::Installation>& Installations() const;
    const std::vector<server::Instance>& Instances() const;

    bool Create(const CreateOptions& options, std::string& error);
    bool Remove(int index, std::string& error);
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

private:
    bool EnsureFile(const server::Instance& instance, std::string& error);
    void SyncPids();
    std::string JavaFor(const server::Instance& instance, std::string& error) const;

    std::string game_dir_;
    launch::Environment environment_;
    server::Catalog catalog_;
    std::map<std::string, server::Runner> runners_;
    std::map<std::string, int> exit_codes_;
};
