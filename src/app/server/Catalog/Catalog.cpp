#include "app/server/Catalog/Catalog.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "app/directory/Directory.h"
#include "app/server/Runner/Runner.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace server {

namespace {

bool IsAbsolutePath(const std::string& path) {
    if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
        return true;
    }
    if (!path.empty() && (path[0] == '\\' || path[0] == '/')) return true;
    return false;
}

}  // namespace

Catalog::Catalog(std::string game_dir) : game_dir_(std::move(game_dir)) {}

std::string Catalog::ServersDir(const std::string& game_dir) {
    return Directory(game_dir).ServersRoot();
}

std::string Catalog::InstallationsDir(const std::string& game_dir) {
    return Directory(game_dir).VersionsRoot();
}

std::string Catalog::ConfigPath() { return Directory::ServersFile(); }

void Catalog::Reload() {
    ScanInstallations();
    LoadInstances();
}

void Catalog::ScanInstallations() {
    installations_.clear();

    const std::string root = InstallationsDir(game_dir_);
    if (!tools::IsDirectory(root)) return;

    Json parser;
    for (const std::string& id : tools::ListDirectories(root)) {
        const std::string dir = tools::JoinPath(root, id);
        const std::string marker_path = tools::JoinPath(dir, "rml.json");
        if (!tools::PathExists(marker_path)) continue;

        const std::string text = tools::ReadTextFile(marker_path);
        if (text.empty()) continue;

        Json::Value marker;
        std::string parse_error;
        if (!parser.Parse(text, marker, parse_error)) continue;

        if (marker["kind"].AsString() != "server") continue;

        const std::string server_jar = marker["serverJar"].AsString();
        if (server_jar.empty()) continue;

        const std::string jar = IsAbsolutePath(server_jar) ? server_jar : tools::JoinPath(dir, server_jar);
        if (!tools::PathExists(jar)) continue;

        Installation installation;
        installation.id = id;
        installation.name = marker["name"].AsString();
        if (installation.name.empty()) installation.name = id;
        installation.loader = marker["loader"].AsString();
        installation.minecraft = marker["minecraft"].AsString();
        installation.jar = jar;
        installation.java_major = static_cast<int>(marker["javaMajor"].AsInt(8));
        if (installation.java_major <= 0) installation.java_major = 8;

        // Fabric 服务端：启动主类 + libraries 下的相对 classpath（原版没有这两项）
        installation.main_class = marker["mainClass"].AsString();
        const Json::Value& classpath = marker["classpath"];
        if (classpath.IsArray()) {
            for (size_t i = 0; i < classpath.Size(); ++i) {
                const std::string entry = classpath.At(i).AsString();
                if (!entry.empty()) installation.classpath.push_back(entry);
            }
        }

        installations_.push_back(std::move(installation));
    }

    std::sort(installations_.begin(), installations_.end(),
              [](const Installation& left, const Installation& right) { return left.id < right.id; });
}

void Catalog::LoadInstances() {
    instances_.clear();

    const std::string path = ConfigPath();
    if (!tools::PathExists(path)) return;

    const std::string text = tools::ReadTextFile(path);
    if (text.empty()) return;

    Json::Value root;
    Json parser;
    std::string parse_error;
    if (!parser.Parse(text, root, parse_error)) return;

    const Json::Value& items = root["instances"];
    if (!items.IsArray()) return;

    for (size_t i = 0; i < items.Size(); ++i) {
        const Json::Value& item = items.At(i);
        if (!item.IsObject()) continue;

        Instance instance;
        instance.name = item["name"].AsString();
        instance.dir = item["dir"].AsString();
        if (instance.name.empty() || instance.dir.empty()) continue;

        instance.installation = item["installation"].AsString();
        instance.port = static_cast<int>(item["port"].AsInt(25565));
        instance.memory_mb = static_cast<int>(item["memoryMb"].AsInt(2048));
        instance.gamemode = item["gamemode"].AsString();
        if (instance.gamemode.empty()) instance.gamemode = "survival";
        instance.difficulty = item["difficulty"].AsString();
        if (instance.difficulty.empty()) instance.difficulty = "easy";
        instance.max_players = static_cast<int>(item["maxPlayers"].AsInt(20));
        instance.online_mode = item["onlineMode"].AsBool(false);
        instance.pid = static_cast<int>(item["pid"].AsInt(0));
        if (instance.pid > 0 && !Runner::PidAlive(instance.pid)) instance.pid = 0;

        instances_.push_back(std::move(instance));
    }
}

int Catalog::IndexOf(const std::string& dir) const {
    for (size_t i = 0; i < instances_.size(); ++i) {
        if (instances_[i].dir == dir) return static_cast<int>(i);
    }
    return -1;
}

bool Catalog::Save(std::string& error) const {
    std::string dir_error;
    if (!tools::EnsureDir(Directory::DataRoot(), dir_error)) {
        error = dir_error;
        return false;
    }

    Json::Value instances = Json::Value::Array();
    for (const auto& instance : instances_) {
        Json::Value item = Json::Value::Object();
        item.Set("name", Json::Value::String(instance.name));
        item.Set("dir", Json::Value::String(instance.dir));
        item.Set("installation", Json::Value::String(instance.installation));
        item.Set("port", Json::Value::Int(instance.port));
        item.Set("memoryMb", Json::Value::Int(instance.memory_mb));
        item.Set("gamemode", Json::Value::String(instance.gamemode));
        item.Set("difficulty", Json::Value::String(instance.difficulty));
        item.Set("maxPlayers", Json::Value::Int(instance.max_players));
        item.Set("onlineMode", Json::Value::Bool(instance.online_mode));
        item.Set("pid", Json::Value::Int(instance.pid));
        instances.Push(std::move(item));
    }

    Json::Value root = Json::Value::Object();
    root.Set("instances", std::move(instances));

    const std::string path = ConfigPath();
    if (!tools::WriteTextFile(path, root.Dump())) {
        error = "写入服务端清单失败: " + path;
        return false;
    }
    return true;
}

}  // namespace server
