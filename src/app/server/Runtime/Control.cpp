#include "app/server/Runtime/Control.h"

#include <fstream>
#include <iterator>
#include <utility>

#include "app/directory/Directory.h"
#include "app/tools/tools.h"

namespace server {
namespace runtime {

namespace {

// Forge 的 win_args.txt 用相对安装目录的 libraries/...；实例目录不同，需要改写成绝对路径。
bool RewriteForgeArgs(const std::string& source, const std::string& install_dir,
                      const std::string& dest, std::string& error) {
    std::ifstream in(tools::ToPath(source), std::ios::binary);
    if (!in) {
        error = "读不到 Forge 参数文件: " + source;
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    std::string rewritten;
    const std::string needle = "libraries/";
    size_t begin = 0;
    while (true) {
        const size_t pos = text.find(needle, begin);
        if (pos == std::string::npos) {
            rewritten.append(text, begin, std::string::npos);
            break;
        }
        rewritten.append(text, begin, pos - begin);
        rewritten += tools::JoinPath(install_dir, "libraries") + "/";
        begin = pos + needle.size();
    }

    const size_t slash = dest.find_last_of('/');
    if (slash != std::string::npos) {
        std::string dir_error;
        if (!tools::EnsureDir(dest.substr(0, slash), dir_error)) {
            error = dir_error;
            return false;
        }
    }
    std::ofstream out(tools::ToPath(dest), std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "写 Forge 参数文件失败: " + dest;
        return false;
    }
    out.write(rewritten.data(), static_cast<std::streamsize>(rewritten.size()));
    if (!out) {
        error = "写 Forge 参数文件失败: " + dest;
        return false;
    }
    return true;
}

}  // namespace

bool Control::Sync(std::vector<Instance>& instances) {
    // 已经退出的 Runner 记下退出码，然后丢掉句柄；进程号留给下面按存活情况清理
    for (auto it = runners_.begin(); it != runners_.end();) {
        if (it->second.IsRunning()) {
            ++it;
            continue;
        }
        const int code = it->second.ExitCode();
        if (code >= 0) exit_codes_[it->first] = code;
        it = runners_.erase(it);
    }

    bool changed = false;
    for (auto& instance : instances) {
        auto runner = runners_.find(instance.dir);
        if (runner != runners_.end() && runner->second.IsRunning()) {
            if (instance.pid != runner->second.Pid()) {
                instance.pid = runner->second.Pid();
                changed = true;
            }
            continue;
        }
        if (instance.pid > 0 && !server::Runner::PidAlive(instance.pid)) {
            instance.pid = 0;
            changed = true;
        }
    }
    return changed;
}

bool Control::Running(const Instance& instance) const {
    const auto runner = runners_.find(instance.dir);
    if (runner != runners_.end() && runner->second.IsRunning()) return true;
    return instance.pid > 0 && server::Runner::PidAlive(instance.pid);
}

int Control::Pid(const Instance& instance) const {
    const auto runner = runners_.find(instance.dir);
    if (runner != runners_.end() && runner->second.Pid() > 0) return runner->second.Pid();
    return instance.pid;
}

std::string Control::Status(const Instance& instance) const {
    if (Running(instance)) return "运行中";

    const auto code = exit_codes_.find(instance.dir);
    if (code != exit_codes_.end()) {
        return "已停止（退出码 " + std::to_string(code->second) + "）";
    }
    const auto runner = runners_.find(instance.dir);
    if (runner != runners_.end() && !runner->second.IsRunning()) {
        const int exit_code = runner->second.ExitCode();
        if (exit_code >= 0) return "已停止（退出码 " + std::to_string(exit_code) + "）";
    }
    return "未启动";
}

bool Control::Start(const Instance& instance, const Catalog::Installation& installation,
                    const std::string& java, const std::string& game_dir, std::string& error) {
    const std::string instance_dir = Directory(game_dir).ServerDir(instance.dir);
    const std::string install_dir = Directory(game_dir).VersionDir(installation.id);

    // 原版：java -jar server.jar；Fabric：java -cp <libraries>;<server.jar> <mainClass>
    // Forge：java @<改写后的 win_args.txt>
    std::vector<std::string> args;
    args.push_back("-Xmx" + std::to_string(instance.memory_mb) + "M");
    if (!installation.arg_file.empty()) {
        const std::string source = tools::JoinPath(install_dir, installation.arg_file);
        const std::string dest = tools::JoinPath(instance_dir, "forge_args.txt");
        if (!RewriteForgeArgs(source, install_dir, dest, error)) return false;
        args.push_back("@" + dest);
    } else if (installation.main_class.empty()) {
        args.push_back("-jar");
        args.push_back(installation.jar);
        args.push_back("nogui");
    } else {
        std::string classpath;
        for (const auto& relative : installation.classpath) {
            const std::string full = tools::JoinPath(Directory(game_dir).LibrariesRoot(), relative);
            if (!tools::PathExists(full)) {
                error = "Fabric 服务端依赖不存在: " + full;
                return false;
            }
            if (!classpath.empty()) classpath += ";";
            classpath += full;
        }
        if (!classpath.empty()) classpath += ";";
        classpath += installation.jar;

        args.push_back("-cp");
        args.push_back(classpath);
        args.push_back(installation.main_class);
        args.push_back("nogui");
    }

    const std::string log_dir = tools::JoinPath(instance_dir, "logs");
    std::string log_error;
    if (!tools::EnsureDir(log_dir, log_error)) {
        error = "创建服务器日志目录失败: " + log_dir + "（" + log_error + "）";
        return false;
    }

    const std::string log_path = tools::JoinPath(log_dir, "console.log");
    auto runner = runners_.try_emplace(instance.dir).first;
    if (!runner->second.Start(java, args, instance_dir, log_path, error)) {
        runners_.erase(runner);
        return false;
    }

    exit_codes_.erase(instance.dir);
    return true;
}

bool Control::Stop(const Instance& instance, bool force, std::string& error) {
    auto runner = runners_.find(instance.dir);
    if (runner == runners_.end() || !runner->second.IsRunning()) {
        // 启动器重启过，只记得 pid：只能按进程号强杀
        if (!force) {
            error = "启动器重启过，无法优雅停止，请使用强制停止";
            return false;
        }
        if (!server::Runner::KillPid(instance.pid, error)) return false;

        exit_codes_[instance.dir] = 1;
        return true;
    }

    if (force) {
        if (!runner->second.ForceStop(error)) return false;
        exit_codes_[instance.dir] = 1;
        runners_.erase(runner);
        return true;
    }

    // 优雅停止：只发 stop，pid 留着等进程自己退出
    return runner->second.Stop(error);
}

bool Control::Send(const Instance& instance, const std::string& command, std::string& error) {
    auto runner = runners_.find(instance.dir);
    if (runner == runners_.end() || !runner->second.IsRunning()) {
        error = "服务器没有运行";
        return false;
    }
    return runner->second.Send(command, error);
}

std::string Control::Console(const std::string& instance_dir, size_t max_bytes) const {
    if (max_bytes == 0) return {};

    const std::string path = tools::JoinPath(tools::JoinPath(instance_dir, "logs"), "console.log");
    if (!tools::PathExists(path)) return {};

    // 只读尾部：服务端日志可能很大，不能每秒整文件读一遍
    std::ifstream stream(tools::ToPath(path), std::ios::binary);
    if (!stream) return {};

    stream.seekg(0, std::ios::end);
    const std::streamoff size = stream.tellg();
    const std::streamoff want = static_cast<std::streamoff>(max_bytes);
    if (size > want) {
        stream.seekg(size - want, std::ios::beg);
    } else {
        stream.seekg(0, std::ios::beg);
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void Control::Forget(const std::string& dir) {
    runners_.erase(dir);
    exit_codes_.erase(dir);
}

void Control::Renamed(const std::string& old_dir, const std::string& new_dir) {
    runners_.erase(old_dir);
    const auto code = exit_codes_.find(old_dir);
    if (code != exit_codes_.end()) {
        const int exit_code = code->second;
        exit_codes_.erase(code);
        exit_codes_[new_dir] = exit_code;
    }
}

}  // namespace runtime
}  // namespace server
