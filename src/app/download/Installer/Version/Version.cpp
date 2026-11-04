#include "app/download/Installer/Version/Version.h"

#include <filesystem>

#include "app/directory/Directory.h"
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

}  // namespace

bool Version::BuildCoreTasks(const launch::Version::Detail& detail,
                             std::vector<Transfer::Task>& out, std::string& error) const {
    (void)error;
    const Directory& paths = version_.Paths();
    const std::string selected = version_.Selected();

    if (!detail.client_url.empty()) {
        Transfer::Task task;
        task.url = detail.client_url;
        task.sha1 = detail.client_sha1;
        // 继承来的客户端 jar（Forge 继承原版）已经在原版目录里，别再拷一份到加载器目录
        task.dest = detail.client_jar.empty() ? paths.VersionJar(selected) : detail.client_jar;
        task.label = selected + ".jar";
        task.size = detail.client_size;
        out.push_back(std::move(task));
    }

    const std::string libraries_root = paths.LibrariesRoot();
    for (const auto& library : detail.libraries) {
        if (!MatchesRules(library.rules)) continue;
        std::string relative = library.path.empty() ? tools::MavenToPath(library.name) : library.path;
        if (!relative.empty() && !library.url.empty()) {
            Transfer::Task task;
            task.url = library.url;
            task.sha1 = library.sha1;
            task.dest = tools::JoinPath(libraries_root, relative);
            task.label = relative;
            task.size = library.size;
            out.push_back(std::move(task));
        }

        // 老版本的 natives 是 classifier jar，也得下（1.13 之后就没了）
        auto native = library.natives.find("windows");
        if (native == library.natives.end()) continue;

        std::string classifier = native->second;
        const std::string arch = "64";  // 与 Starter 一致：x86_64
        const std::string needle = "${arch}";
        const size_t pos = classifier.find(needle);
        if (pos != std::string::npos) classifier.replace(pos, needle.size(), arch);

        auto file = library.classifiers.find(classifier);
        if (file == library.classifiers.end() || file->second.url.empty()) continue;

        Transfer::Task task;
        task.url = file->second.url;
        task.sha1 = file->second.sha1;
        task.dest = tools::JoinPath(libraries_root, file->second.path);
        task.label = file->second.path;
        task.size = file->second.size;
        out.push_back(std::move(task));
    }

    if (!detail.asset_index_url.empty()) {
        Transfer::Task task;
        task.url = detail.asset_index_url;
        task.sha1 = detail.asset_index_sha1;
        task.dest = paths.AssetIndexesRoot() + "/" + detail.asset_index + ".json";
        task.label = "assets/indexes/" + detail.asset_index + ".json";
        out.push_back(std::move(task));
    }
    return true;
}

bool Version::BuildAssetTasks(const launch::Version::Detail& detail, const std::string& assets_root,
                              std::vector<Transfer::Task>& out, std::string& error) const {
    const Directory paths(version_.GameDir());
    const std::string index_path = paths.AssetIndexesRoot() + "/" + detail.asset_index + ".json";
    const std::string text = Directory::ReadTextFile(index_path);
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

    const std::string objects_root = paths.AssetObjectsRoot();
    const std::string base =
        assets_root.empty() ? "https://resources.download.minecraft.net" : assets_root;
    for (size_t i = 0; i < objects.Size(); ++i) {
        const Json::Value& object = objects.At(i);
        std::string hash = object["hash"].AsString();
        if (hash.size() < 3) continue;

        Transfer::Task task;
        task.url = tools::JoinPath(base, hash.substr(0, 2) + "/" + hash);
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
                      Transfer::Result& result, std::string& error, bool apply_defaults,
                      std::string assets_root) const {
    launch::Version::Detail detail;
    if (!version_.LoadDetail(detail, error)) return false;

    std::vector<Transfer::Task> core;
    BuildCoreTasks(detail, core, error);
    if (!transfer.Run(core, progress, result, error)) return false;

    // 资源索引落地之后才知道有哪些对象
    std::vector<Transfer::Task> assets;
    if (!BuildAssetTasks(detail, assets_root, assets, error)) return false;
    if (!transfer.Run(assets, progress, result, error)) return false;

    if (!ExtractNatives(detail, error)) return false;

    if (apply_defaults) ApplyDefaults(version_.GameDir());
    return true;
}

bool Version::WriteMarker(const std::string& game_dir, const std::string& id,
                          const std::string& loader, bool complete, bool user_installed,
                          bool write_user) {
    const Directory paths(game_dir);
    const std::string dir = paths.VersionDir(id);
    std::string error;
    if (!tools::EnsureDir(dir, error)) return false;

    Json::Value root = Json::Value::Object();
    root.Set("managed", Json::Value::Bool(true));
    root.Set("loader", Json::Value::String(loader));
    root.Set("complete", Json::Value::Bool(complete));
    if (write_user) root.Set("user", Json::Value::Bool(user_installed));
    return Directory::WriteTextFile(paths.VersionDir(id) + "/rml.json", root.Dump());
}

bool Version::ExtractNatives(const launch::Version::Detail& detail, std::string& error) const {
    const std::string selected = version_.Selected();
    const Directory& paths = version_.Paths();
    const std::string natives_dir = paths.NativesDir(selected);
    const std::string libraries_root = paths.LibrariesRoot();

    bool any = false;
    for (const auto& library : detail.libraries) {
        if (library.natives.find("windows") == library.natives.end()) continue;

        std::string classifier = library.natives.at("windows");
        const std::string needle = "${arch}";
        const size_t pos = classifier.find(needle);
        if (pos != std::string::npos) classifier.replace(pos, needle.size(), "64");

        auto file = library.classifiers.find(classifier);
        if (file == library.classifiers.end() || file->second.path.empty()) continue;

        const std::string jar = tools::JoinPath(libraries_root, file->second.path);
        if (!tools::PathExists(jar)) continue;  // 缺文件由下载阶段报告
        any = true;

        if (!tools::ExtractArchive(jar, natives_dir, error)) {
            error = "解压 natives 失败(" + file->second.path + "): " + error;
            return false;
        }
    }

    // LWJGL 3 的 jar 里 dll 在 windows/x64/... 下面，java.library.path 只认根目录，平铺一下
    namespace fs = std::filesystem;
    std::error_code walk_error;
    for (const auto& entry : fs::recursive_directory_iterator(tools::ToPath(natives_dir), walk_error)) {
        if (walk_error || !entry.is_regular_file()) continue;
        if (entry.path().extension() != u8".dll") continue;
        if (entry.path().parent_path() == tools::ToPath(natives_dir)) continue;

        std::error_code move_error;
        fs::rename(entry.path(), tools::ToPath(natives_dir) / entry.path().filename(), move_error);
    }

    // 没有 natives 的版本也给个空目录，java.library.path 指着它就行
    if (!any) tools::EnsureDir(natives_dir, error);
    return true;
}

void Version::ApplyDefaults(const std::string& game_dir) {
    // Forge/Fabric 首次启动前就先建好 mods，避免用户找不到放 Mod 的位置
    std::string mods_error;
    Directory(game_dir).EnsureModsRoot(mods_error);

    const std::string path = Directory(game_dir).GameFile("options.txt");
    const std::string text = Directory::ReadTextFile(path);

    // 新装：直接给一份最小配置，游戏自己会补全其余项
    if (text.empty()) {
        Directory::WriteTextFile(path, "lang:zh_cn\n");
        return;
    }

    std::string out;
    bool replaced = false;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        std::string line = end == std::string::npos ? text.substr(start) : text.substr(start, end - start);

        if (line.rfind("lang:", 0) == 0) {
            line = "lang:zh_cn";
            replaced = true;
        }
        out += line;
        out += "\n";

        if (end == std::string::npos) break;
        start = end + 1;
    }

    if (!replaced) out = "lang:zh_cn\n" + out;
    Directory::WriteTextFile(path, out);
}

bool Version::WriteVersionJson(const std::string& game_dir, const std::string& id,
                               const std::string& json, std::string& error) {
    const Directory paths(game_dir);
    const std::string version_dir = paths.VersionDir(id);
    if (!tools::EnsureDir(version_dir, error)) return false;
    const std::string path = paths.VersionJson(id);
    if (!Directory::WriteTextFile(path, json)) {
        error = "写入失败: " + path;
        return false;
    }
    return true;
}

}  // namespace installer
}  // namespace download
