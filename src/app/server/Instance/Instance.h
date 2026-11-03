// 服务端实例：一个运行目录 + 一组启动参数。持久化在 %APPDATA%\RML\servers.json。
#pragma once

#include <string>

namespace server {

struct Instance {
    std::string name;          // 显示名
    std::string dir;           // 相对 <gameDir>/servers 的目录名
    std::string installation;  // 服务端安装的版本目录名（versions/<id>）
    int port = 25565;
    int memory_mb = 2048;
    std::string gamemode = "survival";
    std::string difficulty = "easy";
    int max_players = 20;
    // 启动器目前用离线账号启动游戏，默认关闭正版验证；需要正版验证时在面板里打开
    bool online_mode = false;
    int pid = 0;  // 上次记录的进程号，0 表示没在跑
};

}  // namespace server
