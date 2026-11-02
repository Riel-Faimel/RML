// 组件：启动。用版本管理和环境管理给出的东西，装配运行时并打开游戏。
#pragma once

#include <string>
#include <vector>

#include "include/AccountProvider.h"
#include "app/launch/Environment/Environment.h"
#include "app/launch/Version/Version.h"

namespace launch {

class Starter {
public:
    struct Options {
        int memory_mb = 4096;
        int width = 854;
        int height = 480;
        std::string quick_play;  // host:port，空表示不直连
        std::string java_path;   // 手动指定 Java；空表示按版本要求自动选
        std::string gc;          // 自动 / G1 / ZGC / Parallel
        std::string jvm_args;    // 额外 JVM 参数，空格分隔
        std::string launcher_name = "RML";
        std::string launcher_version = "0.1";
    };

    struct Missing {
        std::string what;
        std::string hint;
    };

    struct Command {
        std::string java;
        std::vector<std::string> args;
        std::string working_dir;
        std::string preview() const;
    };

    Starter(const Version& version, const Environment& environment)
        : version_(version), environment_(environment) {}

    // 组装命令；缺东西时往 missing 里塞，不自动下载。
    bool Build(const AccountProvider::Credentials& credentials, const Options& options, Command& out,
               std::vector<Missing>& missing, std::string& error) const;

    bool Start(const Command& command, std::string& error);
    bool IsRunning() const;
    int ExitCode() const;

private:
    const Version& version_;
    const Environment& environment_;

    void* process_ = nullptr;
};

}  // namespace launch
