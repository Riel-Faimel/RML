// 服务端进程：启动参数装配、stdin 命令、停止与控制台尾部。
#pragma once

#include <map>
#include <string>
#include <vector>

#include "app/server/Catalog/Catalog.h"
#include "app/server/Instance/Instance.h"
#include "app/server/Runner/Runner.h"

namespace server {
namespace runtime {

class Control {
public:
    // 丢掉已退出的进程并记下退出码；把存活 pid 同步进 instances，返回是否有改动
    bool Sync(std::vector<Instance>& instances);

    bool Running(const Instance& instance) const;
    int Pid(const Instance& instance) const;
    std::string Status(const Instance& instance) const;

    bool Start(const Instance& instance, const Catalog::Installation& installation,
               const std::string& java, const std::string& game_dir, std::string& error);
    bool Stop(const Instance& instance, bool force, std::string& error);
    bool Send(const Instance& instance, const std::string& command, std::string& error);

    std::string Console(const std::string& instance_dir, size_t max_bytes) const;

    void Forget(const std::string& dir);
    void Renamed(const std::string& old_dir, const std::string& new_dir);

private:
    std::map<std::string, server::Runner> runners_;
    std::map<std::string, int> exit_codes_;
};

}  // namespace runtime
}  // namespace server
