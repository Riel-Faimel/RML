#include "app/server/Catalog/Catalog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <system_error>
#include <utility>

#include "app/directory/Directory.h"
#include "app/json/JsonArena.h"
#include "app/server/Runner/Runner.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace server {

namespace {

namespace fs = std::filesystem;

bool IsAbsolutePath(const std::string& path) {
    if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
        return true;
    }
    if (!path.empty() && (path[0] == '\\' || path[0] == '/')) return true;
    return false;
}

struct LegacyInstance {
    std::string name;
    std::string dir;
    std::string installation;
    int port = 25565;
    int memory_mb = 2048;
    std::string gamemode = "survival";
    std::string difficulty = "easy";
    int max_players = 20;
    bool online_mode = false;
    int pid = 0;
};

bool WriteManifest(const std::string& path, const Instance& instance, std::string& error) {
    auto j = JsonArena::Instance().Open(path);
    if (!j) {
        error = j.error();
        return false;
    }
    j["schema"] = Json::Value::Int(2);
    j["kind"] = Json::Value::String("serverInstance");
    j["id"] = Json::Value::String(instance.dir);
    j["name"] = Json::Value::String(instance.name);
    j["install"] = Json::Value::String(instance.installation);
    j["state"] = Json::Value::String("committed");
    j["complete"] = Json::Value::Bool(true);
    j["fresh"] = Json::Value::Bool(false);
    j["runtime"]["port"] = Json::Value::Int(instance.port);
    j["runtime"]["memoryMb"] = Json::Value::Int(instance.memory_mb);
    j["runtime"]["gamemode"] = Json::Value::String(instance.gamemode);
    j["runtime"]["difficulty"] = Json::Value::String(instance.difficulty);
    j["runtime"]["maxPlayers"] = Json::Value::Int(instance.max_players);
    j["runtime"]["onlineMode"] = Json::Value::Bool(instance.online_mode);
    j["runtime"]["seed"] = Json::Value::String(instance.seed);
    return j.commit(error);
}

std::vector<LegacyInstance> ReadLegacy() {
    std::vector<LegacyInstance> out;
    const std::string path = Catalog::ConfigPath();
    if (!tools::PathExists(path)) return out;

    Json::Value root;
    Json json;
    std::string error;
    if (!json.Parse(Directory::ReadTextFile(path), root, error)) return out;
    const Json::Value& items = root["instances"];
    if (!items.IsArray()) return out;

    for (size_t i = 0; i < items.Size(); ++i) {
        const Json::Value& item = items.At(i);
        if (!item.IsObject()) continue;
        LegacyInstance legacy;
        legacy.name = item["name"].AsString();
        legacy.dir = item["dir"].AsString();
        legacy.installation = item["installation"].AsString();
        legacy.port = static_cast<int>(item["port"].AsInt(25565));
        legacy.memory_mb = static_cast<int>(item["memoryMb"].AsInt(2048));
        legacy.gamemode = item["gamemode"].AsString();
        if (legacy.gamemode.empty()) legacy.gamemode = "survival";
        legacy.difficulty = item["difficulty"].AsString();
        if (legacy.difficulty.empty()) legacy.difficulty = "easy";
        legacy.max_players = static_cast<int>(item["maxPlayers"].AsInt(20));
        legacy.online_mode = item["onlineMode"].AsBool(false);
        legacy.pid = static_cast<int>(item["pid"].AsInt(0));
        out.push_back(std::move(legacy));
    }
    return out;
}

void BackupLegacy() {
    const std::string source = Catalog::ConfigPath();
    if (!tools::PathExists(source)) return;
    const std::string backup = Directory::DataFile("servers.json.legacy.bak");
    if (tools::PathExists(backup)) return;
    std::error_code ec;
    fs::copy_file(tools::ToPath(source), tools::ToPath(backup), fs::copy_options::none, ec);
}

std::map<std::string, int> ReadRunState() {
    std::map<std::string, int> pids;
    const std::string text = Directory::ReadTextFile(Catalog::RunStatePath());
    if (text.empty()) return pids;
    Json::Value root;
    Json json;
    std::string error;
    if (!json.Parse(text, root, error)) return pids;
    const Json::Value& items = root["pids"];
    for (size_t i = 0; i < items.Size(); ++i) {
        const std::string dir = items.KeyAt(i);
        const int pid = static_cast<int>(items.At(i).AsInt(0));
        if (!dir.empty() && pid > 0) pids[dir] = pid;
    }
    return pids;
}

void ReadRuntime(const JsonProxy& j, Instance& instance) {
    const Json::Value& runtime = j.Get("runtime");
    instance.port = static_cast<int>(runtime["port"].AsInt(j.Get("port").AsInt(25565)));
    instance.memory_mb =
        static_cast<int>(runtime["memoryMb"].AsInt(j.Get("memoryMb").AsInt(2048)));
    instance.gamemode = runtime["gamemode"].AsString();
    if (instance.gamemode.empty() && j.Get("gamemode").IsString()) {
        instance.gamemode = j.Get("gamemode").AsString();
    }
    if (instance.gamemode.empty()) instance.gamemode = "survival";
    instance.difficulty = runtime["difficulty"].AsString();
    if (instance.difficulty.empty() && j.Get("difficulty").IsString()) {
        instance.difficulty = j.Get("difficulty").AsString();
    }
    if (instance.difficulty.empty()) instance.difficulty = "easy";
    instance.max_players =
        static_cast<int>(runtime["maxPlayers"].AsInt(j.Get("maxPlayers").AsInt(20)));
    instance.online_mode = runtime["onlineMode"].AsBool(j.Get("onlineMode").AsBool(false));
    instance.seed = runtime["seed"].AsString();
}

}  // namespace

Catalog::Catalog(std::string game_dir) : game_dir_(std::move(game_dir)) {}

std::string Catalog::ServersDir(const std::string& game_dir) {
    return Directory(game_dir).ServersRoot();
}

std::string Catalog::InstallationsDir(const std::string& game_dir) {
    return Directory(game_dir).VersionsRoot();
}

std::string Catalog::ManifestPath(const std::string& game_dir, const std::string& dir) {
    return Directory(game_dir).ServerManifest(dir);
}

std::string Catalog::ConfigPath() { return Directory::ServersFile(); }

std::string Catalog::RunStatePath() { return Directory::ServerRunStateFile(); }

void Catalog::Reload() {
    ScanInstallations();
    LoadInstances();
}

void Catalog::ScanInstallations() {
    installations_.clear();

    const std::string root = InstallationsDir(game_dir_);
    if (!tools::IsDirectory(root)) return;

    for (const std::string& id : tools::ListDirectories(root)) {
        const std::string dir = tools::JoinPath(root, id);
        const std::string marker_path = tools::JoinPath(dir, "rml.json");
        if (!tools::PathExists(marker_path)) continue;

        auto j = JsonArena::Instance().Open(marker_path);
        if (!j) continue;
        const std::string role = j.Get("role").AsString();
        if (role != "server" && j.Get("kind").AsString() != "server") continue;

        const std::string server_jar = j.Get("serverJar").AsString();
        const std::string arg_file = j.Get("argFile").AsString();
        if (server_jar.empty() && arg_file.empty()) continue;

        std::string jar;
        if (!server_jar.empty()) {
            jar = IsAbsolutePath(server_jar) ? server_jar : tools::JoinPath(dir, server_jar);
            if (!tools::PathExists(jar)) continue;
        }
        if (!arg_file.empty() && !tools::PathExists(tools::JoinPath(dir, arg_file))) continue;

        Installation installation;
        installation.id = id;
        installation.name = j.Get("name").AsString();
        if (installation.name.empty()) installation.name = id;
        installation.loader = j.Get("loader").AsString();
        installation.minecraft = j.Get("minecraft").AsString();
        installation.core = j.Get("core").AsString();
        installation.jar = jar;
        installation.arg_file = arg_file;
        installation.java_major = static_cast<int>(j.Get("javaMajor").AsInt(8));
        if (installation.java_major <= 0) installation.java_major = 8;

        installation.main_class = j.Get("mainClass").AsString();
        const Json::Value& classpath = j.Get("classpath");
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

    const std::string servers = ServersDir(game_dir_);
    if (!tools::IsDirectory(servers)) return;

    const std::vector<LegacyInstance> legacy = ReadLegacy();
    if (!legacy.empty() || tools::PathExists(ConfigPath())) BackupLegacy();
    std::map<std::string, const LegacyInstance*> legacy_by_dir;
    for (const auto& item : legacy) {
        if (!item.dir.empty()) legacy_by_dir[item.dir] = &item;
    }
    const std::map<std::string, int> pids = ReadRunState();

    for (const std::string& dir : tools::ListDirectories(servers)) {
        const std::string manifest = ManifestPath(game_dir_, dir);
        Instance instance;
        instance.dir = dir;
        bool loaded = false;
        const bool manifest_exists = tools::PathExists(manifest);

        if (manifest_exists) {
            auto j = JsonArena::Instance().Open(manifest);
            if (j && j.Get("kind").AsString() == "serverInstance") {
                instance.name = j.Get("name").AsString();
                instance.installation = j.Get("install").AsString();
                ReadRuntime(j, instance);
                loaded = true;
            }
        }

        if (!loaded) {
            auto found = legacy_by_dir.find(dir);
            if (found != legacy_by_dir.end()) {
                const LegacyInstance& old = *found->second;
                instance.name = old.name;
                instance.installation = old.installation;
                instance.port = old.port;
                instance.memory_mb = old.memory_mb;
                instance.gamemode = old.gamemode;
                instance.difficulty = old.difficulty;
                instance.max_players = old.max_players;
                instance.online_mode = old.online_mode;
                if (instance.name.empty()) instance.name = dir;
            } else {
                // 目录存在但没有 manifest / 旧记录：保留实例，按默认值补一份。
                instance.name = dir;
            }
            // 已有但解析不了的 manifest 不覆盖，避免把非法记录洗掉。
            if (!manifest_exists) {
                std::string error;
                WriteManifest(manifest, instance, error);
            }
        }
        if (instance.name.empty()) instance.name = dir;

        int pid = 0;
        auto cached = pids.find(dir);
        if (cached != pids.end()) pid = cached->second;
        if (pid == 0) {
            auto found = legacy_by_dir.find(dir);
            if (found != legacy_by_dir.end()) pid = found->second->pid;
        }
        if (pid > 0 && !Runner::PidAlive(pid)) pid = 0;
        instance.pid = pid;
        instances_.push_back(std::move(instance));
    }

    std::sort(instances_.begin(), instances_.end(),
              [](const Instance& left, const Instance& right) { return left.dir < right.dir; });

    // install.refs：服务端实例对安装的引用数。
    for (auto& installation : installations_) {
        installation.refs = 0;
        for (const auto& instance : instances_) {
            if (instance.installation == installation.id) ++installation.refs;
        }
    }
}

int Catalog::IndexOf(const std::string& dir) const {
    for (size_t i = 0; i < instances_.size(); ++i) {
        if (instances_[i].dir == dir) return static_cast<int>(i);
    }
    return -1;
}

bool Catalog::SaveInstance(size_t index, std::string& error) const {
    if (index >= instances_.size()) {
        error = "服务器实例不存在";
        return false;
    }
    const Instance& instance = instances_[index];
    return WriteManifest(ManifestPath(game_dir_, instance.dir), instance, error);
}

bool Catalog::Save(std::string& error) const {
    error.clear();
    for (size_t i = 0; i < instances_.size(); ++i) {
        std::string item_error;
        if (!SaveInstance(i, item_error)) {
            if (!error.empty()) error += "\n";
            error += item_error;
        }
    }
    return error.empty();
}

bool Catalog::SaveRunState(std::string& error) const {
    Json::Value pids = Json::Value::Object();
    for (const auto& instance : instances_) {
        if (instance.pid > 0) pids.Set(instance.dir, Json::Value::Int(instance.pid));
    }
    Json::Value root = Json::Value::Object();
    root.Set("pids", std::move(pids));
    if (!Directory::WriteDataFile("server-runstate.json", root.Dump())) {
        error = "写入服务端运行状态失败: " + RunStatePath();
        return false;
    }
    return true;
}

}  // namespace server
