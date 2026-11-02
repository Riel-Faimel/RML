#include "app/download/Installer/Version/Version.h"

#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace download {
namespace installer {

namespace {

constexpr const char* kOsName = "windows";

bool MatchesRules(const Json::Value& rules) {
    if (!rules.IsArray() || rules.Size() == 0) return true;
    bool allowed = false;
    for (size_t i = 0; i < rules.Size(); ++i) {
        const Json::Value& rule = rules.At(i);
        const Json::Value& os = rule["os"];
        if (os.IsObject()) {
            std::string name = os["name"].AsString();
            if (!name.empty() && name != kOsName) continue;
            std::string arch = os["arch"].AsString();
            if (!arch.empty() && arch != "x86_64") continue;
        }
        allowed = rule["action"].AsString() == "allow";
    }
    return allowed;
}

std::string MavenToPath(const std::string& name) {
    std::string text = name;
    std::string extension = "jar";
    size_t at = text.find('@');
    if (at != std::string::npos) {
        extension = text.substr(at + 1);
        text.resize(at);
    }
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t pos = text.find(':', start);
        if (pos == std::string::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    if (parts.size() < 3) return {};
    std::string group = parts[0];
    for (char& c : group) {
        if (c == '.') c = '/';
    }
    std::string file = parts[1] + "-" + parts[2];
    if (parts.size() >= 4) file += "-" + parts[3];
    file += "." + extension;
    return group + "/" + parts[1] + "/" + parts[2] + "/" + file;
}

}  // namespace

bool Version::BuildCoreTasks(const launch::Version::Detail& detail,
                             std::vector<Transfer::Task>& out, std::string& error) const {
    (void)error;
    const std::string game_dir = version_.GameDir();
    const std::string version_dir = version_.VersionDir(detail.id);

    if (!detail.client_url.empty()) {
        Transfer::Task task;
        task.url = detail.client_url;
        task.sha1 = detail.client_sha1;
        task.dest = tools::JoinPath(version_dir, detail.id + ".jar");
        task.label = detail.id + ".jar";
        task.size = detail.client_size;
        out.push_back(std::move(task));
    }

    const std::string libraries_root = tools::JoinPath(game_dir, "libraries");
    for (const auto& library : detail.libraries) {
        if (!MatchesRules(library.rules)) continue;
        std::string relative = library.path.empty() ? MavenToPath(library.name) : library.path;
        if (relative.empty() || library.url.empty()) continue;

        Transfer::Task task;
        task.url = library.url;
        task.sha1 = library.sha1;
        task.dest = tools::JoinPath(libraries_root, relative);
        task.label = relative;
        task.size = library.size;
        out.push_back(std::move(task));
    }

    if (!detail.asset_index_url.empty()) {
        Transfer::Task task;
        task.url = detail.asset_index_url;
        task.sha1 = detail.asset_index_sha1;
        task.dest = tools::JoinPath(tools::JoinPath(tools::JoinPath(game_dir, "assets"), "indexes"),
                                    detail.asset_index + ".json");
        task.label = "assets/indexes/" + detail.asset_index + ".json";
        out.push_back(std::move(task));
    }
    return true;
}

bool Version::BuildAssetTasks(const launch::Version::Detail& detail,
                              std::vector<Transfer::Task>& out, std::string& error) const {
    const std::string game_dir = version_.GameDir();
    const std::string index_path =
        tools::JoinPath(tools::JoinPath(tools::JoinPath(game_dir, "assets"), "indexes"),
                        detail.asset_index + ".json");
    const std::string text = tools::ReadTextFile(index_path);
    if (text.empty()) {
        error = "读不到资源索引: " + index_path;
        return false;
    }

    Json::Value root;
    Json json;
    if (!json.Parse(text, root, error)) return false;

    const Json::Value& objects = root["objects"];
    if (!objects.IsObject()) {
        error = "资源索引里没有 objects";
        return false;
    }

    const std::string objects_root = tools::JoinPath(tools::JoinPath(game_dir, "assets"), "objects");
    for (size_t i = 0; i < objects.Size(); ++i) {
        const Json::Value& object = objects.At(i);
        std::string hash = object["hash"].AsString();
        if (hash.size() < 3) continue;

        Transfer::Task task;
        task.url = "https://resources.download.minecraft.net/" + hash.substr(0, 2) + "/" + hash;
        task.sha1 = hash;
        task.dest = tools::JoinPath(tools::JoinPath(objects_root, hash.substr(0, 2)), hash);
        task.label = "assets/" + hash.substr(0, 2) + "/" + hash;
        task.size = object["size"].AsInt(0);
        out.push_back(std::move(task));
    }
    return true;
}

bool Version::Inspect(std::vector<std::string>& missing, std::string& error) const {
    launch::Version::Detail detail;
    if (!version_.LoadDetail(detail, error)) return false;

    std::vector<Transfer::Task> tasks;
    BuildCoreTasks(detail, tasks, error);
    for (const auto& task : tasks) {
        if (!Transfer::Verify(task.dest, task.sha1)) missing.push_back(task.label);
    }
    return true;
}

bool Version::Install(const Transfer& transfer, const Transfer::Progress& progress,
                      Transfer::Result& result, std::string& error) const {
    launch::Version::Detail detail;
    if (!version_.LoadDetail(detail, error)) return false;

    std::vector<Transfer::Task> core;
    BuildCoreTasks(detail, core, error);
    if (!transfer.Run(core, progress, result, error)) return false;

    // 资源索引落地之后才知道有哪些对象
    std::vector<Transfer::Task> assets;
    if (!BuildAssetTasks(detail, assets, error)) return false;
    return transfer.Run(assets, progress, result, error);
}

bool Version::WriteVersionJson(const std::string& game_dir, const std::string& id,
                               const std::string& json, std::string& error) {
    const std::string version_dir =
        tools::JoinPath(tools::JoinPath(game_dir, "versions"), id);
    if (!tools::EnsureDir(version_dir, error)) return false;
    const std::string path = tools::JoinPath(version_dir, id + ".json");
    if (!tools::WriteTextFile(path, json)) {
        error = "写入失败: " + path;
        return false;
    }
    return true;
}

}  // namespace installer
}  // namespace download
