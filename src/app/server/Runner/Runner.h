// 服务端进程：启动 java、收集控制台日志、发送命令、优雅/强制停止。
// 进程独立运行；启动器只持有句柄，句柄丢了还能按 pid 强杀。
#pragma once

#include <string>
#include <vector>

namespace server {

class Runner {
public:
    Runner() = default;
    ~Runner();

    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;

    // args 是 java 之后的完整参数（-Xmx、-jar / -cp + mainClass、nogui 等）
    bool Start(const std::string& java, const std::vector<std::string>& args,
               const std::string& work_dir, const std::string& log_path, std::string& error);

    bool IsRunning() const;
    int ExitCode() const;
    int Pid() const { return pid_; }

    // 往 stdin 写一行（服务端命令）
    bool Send(const std::string& line, std::string& error);

    // 优雅停止：发 stop
    bool Stop(std::string& error);

    // 强制停止：TerminateProcess
    bool ForceStop(std::string& error);

    static bool PidAlive(int pid);
    static bool KillPid(int pid, std::string& error);

private:
    void CloseHandles();

    void* process_ = nullptr;
    void* stdin_write_ = nullptr;
    int pid_ = 0;
    int exit_code_ = -1;
};

}  // namespace server
