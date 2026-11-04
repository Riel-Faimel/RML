// 组件：环境管理。负责“这台机器上现在有哪些工具可用”——目前是 Java。
// 系统里找到的 Java 只读不改；RML 自己装的放在 %APPDATA%\RML\runtimes，不动系统。
#pragma once

#include <string>
#include <vector>

class NetTask;

namespace launch {

class Environment {
public:
    struct Java {
        std::string path;     // java.exe 绝对路径
        std::string version;  // 21.0.10
        int major = 0;
        std::string source;   // RML / PATH / JAVA_HOME / 注册表 / 目录
    };

    Environment();

    const std::vector<Java>& Javas() const { return javas_; }

    // 需求版本优先完全匹配；否则取「不低于要求的最低版本」（Java 向上兼容）
    const Java* Pick(int required_major) const;
    const Java* Find(const std::string& path) const;

    // RML 自己管理的 Java 根目录：%APPDATA%\RML\runtimes
    static std::string ManagedRoot();

    // 重新扫描（安装完之后调用）
    void Refresh();

    // 下载并解压到 ManagedRoot，不触碰系统
    bool Install(int major, std::string& error);

    // Java 的网络下载交给任务模块；安装/解压仍由这里负责。
    void SetNetTask(NetTask* task_manager) { task_manager_ = task_manager; }

private:
    void Probe();
    void AddCandidate(const std::string& path, const std::string& source);

    std::vector<Java> javas_;
    NetTask* task_manager_ = nullptr;
};

}  // namespace launch
