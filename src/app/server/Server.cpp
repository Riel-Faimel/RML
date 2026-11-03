#include "app/server/Server.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace {

const char* const kManagedKeys[] = {"server-port", "gamemode", "difficulty", "max-players",
                                    "online-mode"};

int ManagedKeyIndex(const std::string& key) {
    for (int i = 0; i < static_cast<int>(std::size(kManagedKeys)); ++i) {
        if (key == kManagedKeys[i]) return i;
    }
    return -1;
}

std::string TrimWhitespace(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) --end;
    return text.substr(begin, end - begin);
}

// 目录名：保留中文/字母数字/._-，其它换成 _；去掉首尾空白和点
std::string SanitizeDirName(const std::string& name) {
    auto trimmable = [](unsigned char c) {
        return std::isspace(c) != 0 || c == '.';
    };

    size_t begin = 0;
    size_t end = name.size();
    while (begin < end && trimmable(static_cast<unsigned char>(name[begin]))) ++begin;
    while (end > begin && trimmable(static_cast<unsigned char>(name[end - 1]))) --end;

    std::string out;
    out.reserve(end - begin);
    for (size_t i = begin; i < end; ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (c >= 0x80 || std::isalnum(c) != 0 || c == '.' || c == '_' || c == '-') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('_');
        }
    }

    // Windows 不允许目录名以点或空格结尾
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
    while (!out.empty() && (out.front() == '.' || out.front() == ' ')) out.erase(out.begin());

    if (out.empty()) out = "server";

    static const char* const kReserved[] = {"con",  "prn",  "aux",  "nul",  "com1", "com2",
                                            "com3", "com4", "com5", "com6", "com7", "com8",
                                            "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5",
                                            "lpt6", "lpt7", "lpt8", "lpt9"};
    const std::string lowered = tools::ToLower(out);
    for (const char* reserved : kReserved) {
        if (lowered == reserved) {
            out += "_";
            break;
        }
    }
    return out;
}

std::string InstanceDir(const std::string& game_dir, const server::Instance& instance) {
    return tools::JoinPath(server::Catalog::ServersDir(game_dir), instance.dir);
}

std::string PropertiesPath(const std::string& game_dir, const server::Instance& instance) {
    return tools::JoinPath(InstanceDir(game_dir, instance), "server.properties");
}

std::map<std::string, std::string> PropertyValues(const server::Instance& instance) {
    return {
        {"server-port", std::to_string(instance.port)},
        {"gamemode", instance.gamemode},
        {"difficulty", instance.difficulty},
        {"max-players", std::to_string(instance.max_players)},
        {"online-mode", instance.online_mode ? "true" : "false"},
    };
}

std::string ManagedLine(const std::map<std::string, std::string>& values,
                        const std::string& key) {
    const auto found = values.find(key);
    return key + "=" + (found != values.end() ? found->second : std::string());
}

// preserve_existing=true 时保留未知行（Update），false 时按固定顺序重写（新建）
std::string BuildProperties(const server::Instance& instance, const std::string& existing,
                            bool preserve_existing) {
    const std::map<std::string, std::string> values = PropertyValues(instance);

    if (!preserve_existing || existing.empty()) {
        std::string out;
        for (const char* key : kManagedKeys) out += ManagedLine(values, key) + "\n";
        return out;
    }

    std::string out;
    bool written[std::size(kManagedKeys)] = {};
    size_t start = 0;
    while (start <= existing.size()) {
        const size_t end = existing.find('\n', start);
        std::string line =
            end == std::string::npos ? existing.substr(start) : existing.substr(start, end - start);

        const size_t equals = line.find('=');
        const std::string key =
            equals == std::string::npos ? TrimWhitespace(line) : TrimWhitespace(line.substr(0, equals));
        const int index = ManagedKeyIndex(key);
        if (index >= 0) {
            if (!written[index]) {
                out += ManagedLine(values, kManagedKeys[index]) + "\n";
                written[index] = true;
            }
            // 重复的受管键只留一个
        } else {
            out += line;
            if (end != std::string::npos) out += "\n";
        }

        if (end == std::string::npos) break;
        start = end + 1;
    }

    if (!out.empty() && out.back() != '\n') out += "\n";
    for (size_t i = 0; i < std::size(kManagedKeys); ++i) {
        if (!written[i]) out += ManagedLine(values, kManagedKeys[i]) + "\n";
    }
    return out;
}

bool WriteProperties(const std::string& game_dir, const server::Instance& instance,
                     bool preserve_existing, std::string& error) {
    const std::string path = PropertiesPath(game_dir, instance);
    const std::string existing = preserve_existing ? tools::ReadTextFile(path) : std::string();
    if (!tools::WriteTextFile(path, BuildProperties(instance, existing, preserve_existing))) {
        error = "写入服务器配置失败: " + path;
        return false;
    }
    return true;
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
    for (auto& instance : catalog_.Instances()) {
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

    if (changed) {
        std::string ignored;
        catalog_.Save(ignored);
    }
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

    const std::string servers = server::Catalog::ServersDir(game_dir_);
    const std::string base = SanitizeDirName(name);
    std::string dir = base;
    int suffix = 2;
    while (catalog_.IndexOf(dir) >= 0 || tools::PathExists(tools::JoinPath(servers, dir))) {
        dir = base + "_" + std::to_string(suffix++);
    }

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

    if (!EnsureFile(instance, error)) return false;

    catalog_.Instances().push_back(std::move(instance));
    if (!catalog_.Save(error)) {
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

    runners_.erase(instance.dir);
    exit_codes_.erase(instance.dir);
    catalog_.Instances().erase(catalog_.Instances().begin() + index);

    if (!catalog_.Save(error)) {
        error = "保存服务器实例失败: " + error;
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

    const std::string base = SanitizeDirName(new_name);
    std::string new_dir = base;
    int suffix = 2;
    while (new_dir != old_dir &&
           (catalog_.IndexOf(new_dir) >= 0 || tools::PathExists(tools::JoinPath(servers, new_dir)))) {
        new_dir = base + "_" + std::to_string(suffix++);
    }

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

        runners_.erase(old_dir);
        const auto code = exit_codes_.find(old_dir);
        if (code != exit_codes_.end()) {
            const int exit_code = code->second;
            exit_codes_.erase(code);
            exit_codes_[new_dir] = exit_code;
        }
    }

    auto& instance = catalog_.Instances()[index];
    instance.name = new_name;
    instance.dir = new_dir;

    if (!catalog_.Save(error)) {
        error = "保存服务器实例失败: " + error;
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

    if (!WriteProperties(game_dir_, updated, true, error)) return false;

    catalog_.Instances()[index] = std::move(updated);
    if (!catalog_.Save(error)) {
        error = "保存服务器实例失败: " + error;
        return false;
    }
    return true;
}

bool Server::ReadProperties(int index, std::vector<std::pair<std::string, std::string>>& out,
                            std::string& error) const {
    out.clear();
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) {
        error = "服务器实例不存在";
        return false;
    }

    const std::string path = PropertiesPath(game_dir_, catalog_.Instances()[index]);
    if (!tools::PathExists(path)) return true;

    const std::string text = tools::ReadTextFile(path);
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        std::string line =
            end == std::string::npos ? text.substr(start) : text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const std::string trimmed = TrimWhitespace(line);
        if (!trimmed.empty() && trimmed[0] != '#' && trimmed[0] != '!') {
            const size_t equals = line.find('=');
            if (equals != std::string::npos) {
                const std::string key = TrimWhitespace(line.substr(0, equals));
                if (!key.empty()) out.emplace_back(key, line.substr(equals + 1));
            } else {
                out.emplace_back(trimmed, std::string());
            }
        }

        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}

bool Server::SaveProperties(int index,
                            const std::vector<std::pair<std::string, std::string>>& entries,
                            std::string& error) {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) {
        error = "服务器实例不存在";
        return false;
    }

    server::Instance updated = catalog_.Instances()[index];

    // 同名键只保留最后一次输入，避免文件里出现重复项
    std::vector<std::pair<std::string, std::string>> unique;
    std::map<std::string, size_t> seen;
    for (const auto& entry : entries) {
        const std::string key = TrimWhitespace(entry.first);
        if (key.empty()) continue;

        std::string value = entry.second;
        for (char& c : value) {
            if (c == '\n' || c == '\r') c = ' ';
        }

        const auto found = seen.find(key);
        if (found != seen.end()) {
            unique[found->second].second = value;
            continue;
        }
        seen.emplace(key, unique.size());
        unique.emplace_back(key, value);
    }

    std::string text;
    for (const auto& entry : unique) {
        const std::string& key = entry.first;
        const std::string& value = entry.second;

        if (key == "server-port") {
            try {
                const int port = std::stoi(TrimWhitespace(value));
                if (port < 1 || port > 65535) throw std::out_of_range("port");
                updated.port = port;
            } catch (...) {
                error = "server-port 必须是 1-65535 的整数: " + value;
                return false;
            }
        } else if (key == "max-players") {
            try {
                const int players = std::stoi(TrimWhitespace(value));
                if (players < 1) throw std::out_of_range("players");
                updated.max_players = players;
            } catch (...) {
                error = "max-players 必须是正整数: " + value;
                return false;
            }
        } else if (key == "gamemode") {
            updated.gamemode = TrimWhitespace(value);
        } else if (key == "difficulty") {
            updated.difficulty = TrimWhitespace(value);
        } else if (key == "online-mode") {
            updated.online_mode = tools::ToLower(TrimWhitespace(value)) == "true";
        }

        text += key + "=" + value + "\n";
    }

    const std::string path = PropertiesPath(game_dir_, updated);
    const size_t slash = path.find_last_of('/');
    if (slash != std::string::npos) {
        std::string dir_error;
        if (!tools::EnsureDir(path.substr(0, slash), dir_error)) {
            error = dir_error;
            return false;
        }
    }
    if (!tools::WriteTextFile(path, text)) {
        error = "写入服务器配置失败: " + path;
        return false;
    }

    catalog_.Instances()[index] = updated;
    if (!catalog_.Save(error)) {
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
    if (!tools::PathExists(installation->jar)) {
        error = "服务端 jar 不存在: " + installation->jar;
        return false;
    }

    // 原版：java -jar server.jar；Fabric：java -cp <libraries>;<server.jar> <mainClass>
    std::vector<std::string> args;
    args.push_back("-Xmx" + std::to_string(instance.memory_mb) + "M");
    if (installation->main_class.empty()) {
        args.push_back("-jar");
        args.push_back(installation->jar);
        args.push_back("nogui");
    } else {
        std::string classpath;
        for (const auto& relative : installation->classpath) {
            const std::string full =
                tools::JoinPath(tools::JoinPath(game_dir_, "libraries"), relative);
            if (!tools::PathExists(full)) {
                error = "Fabric 服务端依赖不存在: " + full;
                return false;
            }
            if (!classpath.empty()) classpath += ";";
            classpath += full;
        }
        if (!classpath.empty()) classpath += ";";
        classpath += installation->jar;

        args.push_back("-cp");
        args.push_back(classpath);
        args.push_back(installation->main_class);
        args.push_back("nogui");
    }

    const std::string java = JavaFor(instance, error);
    if (java.empty()) return false;

    if (!EnsureFile(instance, error)) return false;

    const std::string instance_dir = InstanceDir(game_dir_, instance);
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
    catalog_.Instances()[index].pid = runner->second.Pid();
    if (!catalog_.Save(error)) {
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

    const std::string dir = catalog_.Instances()[index].dir;
    auto runner = runners_.find(dir);
    if (runner == runners_.end() || !runner->second.IsRunning()) {
        // 启动器重启过，只记得 pid：只能按进程号强杀
        const int pid = catalog_.Instances()[index].pid;
        if (!force) {
            error = "启动器重启过，无法优雅停止，请使用强制停止";
            return false;
        }
        if (!server::Runner::KillPid(pid, error)) return false;

        exit_codes_[dir] = 1;
        catalog_.Instances()[index].pid = 0;
        if (!catalog_.Save(error)) {
            error = "服务器已停止，但保存实例状态失败: " + error;
            return false;
        }
        return true;
    }

    if (force) {
        if (!runner->second.ForceStop(error)) return false;
        exit_codes_[dir] = 1;
        runners_.erase(runner);
        catalog_.Instances()[index].pid = 0;
        if (!catalog_.Save(error)) {
            error = "服务器已停止，但保存实例状态失败: " + error;
            return false;
        }
        return true;
    }

    // 优雅停止：只发 stop，pid 留着等进程自己退出
    return runner->second.Stop(error);
}

bool Server::Send(int index, const std::string& command, std::string& error) {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) {
        error = "服务器实例不存在";
        return false;
    }
    auto runner = runners_.find(catalog_.Instances()[index].dir);
    if (runner == runners_.end() || !runner->second.IsRunning()) {
        error = "服务器没有运行";
        return false;
    }
    return runner->second.Send(command, error);
}

bool Server::Running(int index) const {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) return false;
    const server::Instance& instance = catalog_.Instances()[index];
    const auto runner = runners_.find(instance.dir);
    if (runner != runners_.end() && runner->second.IsRunning()) return true;
    return instance.pid > 0 && server::Runner::PidAlive(instance.pid);
}

int Server::Pid(int index) const {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) return 0;
    const server::Instance& instance = catalog_.Instances()[index];
    const auto runner = runners_.find(instance.dir);
    if (runner != runners_.end() && runner->second.Pid() > 0) return runner->second.Pid();
    return instance.pid;
}

std::string Server::Status(int index) const {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) return "实例不存在";
    if (Running(index)) return "运行中";

    const std::string& dir = catalog_.Instances()[index].dir;
    const auto code = exit_codes_.find(dir);
    if (code != exit_codes_.end()) {
        return "已停止（退出码 " + std::to_string(code->second) + "）";
    }
    const auto runner = runners_.find(dir);
    if (runner != runners_.end() && !runner->second.IsRunning()) {
        const int exit_code = runner->second.ExitCode();
        if (exit_code >= 0) return "已停止（退出码 " + std::to_string(exit_code) + "）";
    }
    return "未启动";
}

std::string Server::Console(int index, size_t max_bytes) const {
    if (index < 0 || index >= static_cast<int>(catalog_.Instances().size())) return {};
    if (max_bytes == 0) return {};

    const std::string path =
        tools::JoinPath(tools::JoinPath(InstanceDir(game_dir_, catalog_.Instances()[index]), "logs"),
                        "console.log");
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

bool Server::EnsureFile(const server::Instance& instance, std::string& error) {
    const std::string dir = InstanceDir(game_dir_, instance);

    // 受管参数始终按实例配置重写（未知行保留）；这样旧实例的 online-mode 也会被修正
    if (!WriteProperties(game_dir_, instance, true, error)) return false;

    const std::string eula = tools::JoinPath(dir, "eula.txt");
    if (!tools::PathExists(eula) && !tools::WriteTextFile(eula, "eula=true\n")) {
        error = "写入 EULA 文件失败: " + eula;
        return false;
    }

    // 首次启动时服务端会因为缺名单打 WARN，先放空数组
    for (const char* name :
         {"banned-players.json", "banned-ips.json", "ops.json", "whitelist.json"}) {
        const std::string path = tools::JoinPath(dir, name);
        if (tools::PathExists(path)) continue;
        if (!tools::WriteTextFile(path, "[]\n")) {
            error = "写入服务端名单文件失败: " + path;
            return false;
        }
    }
    return true;
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
