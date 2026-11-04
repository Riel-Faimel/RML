#include "app/server/Properties/Properties.h"

#include <cctype>
#include <iterator>
#include <map>
#include <stdexcept>

#include "app/directory/Directory.h"
#include "app/tools/tools.h"

namespace server {
namespace properties {

namespace {

const char* const kManagedKeys[] = {"server-port", "gamemode", "difficulty", "max-players",
                                    "online-mode"};

std::string InstanceDir(const std::string& game_dir, const Instance& instance) {
    return Directory(game_dir).ServerDir(instance.dir);
}

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

std::map<std::string, std::string> PropertyValues(const Instance& instance) {
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

}  // namespace

std::string File::Path(const std::string& game_dir, const Instance& instance) {
    return tools::JoinPath(InstanceDir(game_dir, instance), "server.properties");
}

// preserve_existing=true 时保留未知行（Update），false 时按固定顺序重写（新建）
std::string File::Build(const Instance& instance, const std::string& existing,
                        bool preserve_existing) {
    const std::map<std::string, std::string> values = PropertyValues(instance);

    if (!preserve_existing || existing.empty()) {
        std::string out;
        for (const char* key : kManagedKeys) out += ManagedLine(values, key) + "\n";
        if (!instance.seed.empty()) {
            std::string seed = instance.seed;
            for (char& c : seed) {
                if (c == '\r' || c == '\n') c = ' ';
            }
            out += "level-seed=" + seed + "\n";
        }
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

bool File::Write(const std::string& game_dir, const Instance& instance, bool preserve_existing,
                 std::string& error) {
    const std::string path = Path(game_dir, instance);
    const std::string existing = preserve_existing ? Directory::ReadTextFile(path) : std::string();
    if (!Directory::WriteTextFile(path, Build(instance, existing, preserve_existing))) {
        error = "写入服务器配置失败: " + path;
        return false;
    }
    return true;
}

bool File::Read(const std::string& game_dir, const Instance& instance,
                std::vector<std::pair<std::string, std::string>>& out, std::string& error) {
    out.clear();
    const std::string path = Path(game_dir, instance);
    if (!tools::PathExists(path)) return true;

    const std::string text = Directory::ReadTextFile(path);
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
    (void)error;
    return true;
}

bool File::Apply(const std::string& game_dir, const Instance& current,
                 const std::vector<std::pair<std::string, std::string>>& entries, Instance& updated,
                 std::string& error) {
    updated = current;

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

    const std::string path = Path(game_dir, updated);
    const size_t slash = path.find_last_of('/');
    if (slash != std::string::npos) {
        std::string dir_error;
        if (!tools::EnsureDir(path.substr(0, slash), dir_error)) {
            error = dir_error;
            return false;
        }
    }
    if (!Directory::WriteTextFile(path, text)) {
        error = "写入服务器配置失败: " + path;
        return false;
    }
    return true;
}

bool File::Ensure(const std::string& game_dir, const Instance& instance, std::string& error) {
    const std::string dir = InstanceDir(game_dir, instance);

    // 受管参数始终按实例配置重写（未知行保留）；这样旧实例的 online-mode 也会被修正
    if (!Write(game_dir, instance, true, error)) return false;

    const std::string eula = tools::JoinPath(dir, "eula.txt");
    if (!tools::PathExists(eula) && !Directory::WriteTextFile(eula, "eula=true\n")) {
        error = "写入 EULA 文件失败: " + eula;
        return false;
    }

    // 首次启动时服务端会因为缺名单打 WARN，先放空数组
    for (const char* name :
         {"banned-players.json", "banned-ips.json", "ops.json", "whitelist.json"}) {
        const std::string path = tools::JoinPath(dir, name);
        if (tools::PathExists(path)) continue;
        if (!Directory::WriteTextFile(path, "[]\n")) {
            error = "写入服务端名单文件失败: " + path;
            return false;
        }
    }
    return true;
}

}  // namespace properties
}  // namespace server
