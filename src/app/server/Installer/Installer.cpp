#include "app/server/Installer/Installer.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace server {

namespace {

bool LoadVanillaJson(download::VersionSource& sources, const std::string& mc_version,
                     Json::Value& root, std::string& error) {
    std::vector<download::versionsource::Mojang::Entry> entries;
    if (!sources.Game().List(entries, error)) {
        error = "拉取版本清单失败: " + error;
        return false;
    }

    std::string url;
    for (const auto& entry : entries) {
        if (entry.id == mc_version) {
            url = entry.url;
            break;
        }
    }
    if (url.empty()) {
        error = "找不到 Minecraft " + mc_version + " 的版本信息";
        return false;
    }

    std::string json_text;
    if (!sources.Game().Fetch(url, json_text, error)) {
        error = "下载 " + mc_version + " 的版本信息失败: " + error;
        return false;
    }

    Json parser;
    std::string parse_error;
    if (!parser.Parse(json_text, root, parse_error)) {
        error = "解析 " + mc_version + " 的版本信息失败: " + parse_error;
        return false;
    }
    return true;
}

std::string JoinUrl(const std::string& base, const std::string& path) {
    if (base.empty()) return path;
    if (base.back() == '/') return base + path;
    return base + "/" + path;
}

// 原版与 Fabric 共用一份 rml.json 写法；Fabric 额外带 mainClass/classpath
bool WriteServerMarker(const launch::Version& versions, const std::string& id,
                       const std::string& name, const std::string& loader,
                       const std::string& mc_version, int java_major,
                       const std::string& main_class, const std::vector<std::string>& classpath,
                       std::string& error) {
    const std::string dir = versions.VersionDir(id);
    std::string dir_error;
    if (!tools::EnsureDir(dir, dir_error)) {
        error = "创建服务端版本目录失败: " + dir + "（" + dir_error + "）";
        return false;
    }

    Json::Value root = Json::Value::Object();
    root.Set("managed", Json::Value::Bool(true));
    root.Set("kind", Json::Value::String("server"));
    root.Set("loader", Json::Value::String(loader));
    root.Set("minecraft", Json::Value::String(mc_version));
    root.Set("serverJar", Json::Value::String("server.jar"));
    root.Set("javaMajor", Json::Value::Int(java_major));
    root.Set("complete", Json::Value::Bool(true));
    root.Set("name", Json::Value::String(name));
    if (!main_class.empty()) root.Set("mainClass", Json::Value::String(main_class));
    if (!classpath.empty()) {
        Json::Value entries = Json::Value::Array();
        for (const auto& relative : classpath) {
            entries.Push(Json::Value::String(relative));
        }
        root.Set("classpath", std::move(entries));
    }

    const std::string path = tools::JoinPath(dir, "rml.json");
    if (!tools::WriteTextFile(path, root.Dump())) {
        error = "写入服务端安装标记失败: " + path;
        return false;
    }
    return true;
}

}  // namespace

Installer::Installer(launch::Version& versions, launch::Environment& environment,
                     download::VersionSource& sources, download::installer::Transfer& transfer)
    : versions_(versions), environment_(environment), sources_(sources), transfer_(transfer) {}

bool Installer::Vanilla(const std::string& mc_version, const Progress& progress,
                        std::string& version_id, std::string& error) const {
    Json::Value root;
    if (!LoadVanillaJson(sources_, mc_version, root, error)) return false;

    const Json::Value& server = root["downloads"]["server"];
    const std::string server_url = server["url"].AsString();
    if (!server.IsObject() || server_url.empty()) {
        error = mc_version + " 没有服务端下载";
        return false;
    }

    int java_major = static_cast<int>(root["javaVersion"]["majorVersion"].AsInt(8));
    if (java_major <= 0) java_major = 8;

    const std::string id = mc_version + "-server";
    const std::string name = mc_version + " 服务端";
    if (!Prepare(id, name, "原版", mc_version, java_major, error)) return false;

    const std::string dir = versions_.VersionDir(id);
    download::installer::Transfer::Task task;
    task.url = server_url;
    task.sha1 = server["sha1"].AsString();
    task.dest = tools::JoinPath(dir, "server.jar");
    task.label = mc_version + "-server.jar";
    task.size = server["size"].AsInt(0);

    download::installer::Transfer::Result result;
    if (!transfer_.Run({task}, progress, result, error)) {
        error = "下载服务端失败（" + task.dest + "）: " + error;
        return false;
    }

    if (!WriteMarker(id, name, "原版", mc_version, java_major, error)) return false;
    version_id = id;
    return true;
}

bool Installer::Fabric(const std::string& mc_version, const Progress& progress,
                       std::string& version_id, std::string& error) const {
    std::string loader_version;
    if (!sources_.Loaders().FabricLatest(mc_version, loader_version, error)) {
        error = "获取 " + mc_version + " 的 Fabric 加载器版本失败: " + error;
        return false;
    }
    if (loader_version.empty()) {
        error = "找不到 " + mc_version + " 可用的 Fabric 加载器版本";
        return false;
    }

    // Fabric 服务端仍然需要原版 server.jar；javaMajor 拿不到就按 17
    Json::Value vanilla;
    if (!LoadVanillaJson(sources_, mc_version, vanilla, error)) return false;

    int java_major = 17;
    const int vanilla_major =
        static_cast<int>(vanilla["javaVersion"]["majorVersion"].AsInt(0));
    if (vanilla_major > 0) java_major = vanilla_major;

    const Json::Value& server = vanilla["downloads"]["server"];
    const std::string server_url = server["url"].AsString();
    if (!server.IsObject() || server_url.empty()) {
        error = mc_version + " 没有服务端下载";
        return false;
    }

    const std::string id = mc_version + "-fabric-server";
    const std::string name = mc_version + " Fabric 服务端";
    if (!Prepare(id, name, "Fabric", mc_version, java_major, error)) return false;

    const std::string dir = versions_.VersionDir(id);
    const std::string game_libraries = tools::JoinPath(versions_.GameDir(), "libraries");

    // 1) 原版 server.jar
    download::installer::Transfer::Task jar_task;
    jar_task.url = server_url;
    jar_task.sha1 = server["sha1"].AsString();
    jar_task.dest = tools::JoinPath(dir, "server.jar");
    jar_task.label = mc_version + "-server.jar";
    jar_task.size = server["size"].AsInt(0);

    download::installer::Transfer::Result result;
    if (!transfer_.Run({jar_task}, progress, result, error)) {
        error = "下载原版服务端失败（" + jar_task.dest + "）: " + error;
        return false;
    }

    // 2) Fabric 服务端 profile：mainClass + libraries
    std::string profile_json;
    if (!sources_.Loaders().FabricServerProfile(mc_version, loader_version, profile_json, error)) {
        error = "获取 " + mc_version + " 的 Fabric 服务端配置失败: " + error;
        return false;
    }

    Json::Value profile;
    Json parser;
    std::string parse_error;
    if (!parser.Parse(profile_json, profile, parse_error)) {
        error = "解析 Fabric 服务端配置失败: " + parse_error;
        return false;
    }

    const std::string main_class = profile["mainClass"].AsString();
    if (main_class.empty()) {
        error = "Fabric 服务端配置里没有 mainClass";
        return false;
    }

    const Json::Value& libraries = profile["libraries"];
    if (!libraries.IsArray()) {
        error = "Fabric 服务端配置里没有 libraries";
        return false;
    }

    std::vector<download::installer::Transfer::Task> library_tasks;
    std::vector<std::string> classpath;
    for (size_t i = 0; i < libraries.Size(); ++i) {
        const Json::Value& library = libraries.At(i);
        const Json::Value& artifact = library["downloads"]["artifact"];

        std::string relative = artifact["path"].AsString();
        if (relative.empty()) relative = tools::MavenToPath(library["name"].AsString());
        if (relative.empty()) {
            error = "Fabric 依赖缺少路径: " + library["name"].AsString();
            return false;
        }

        std::string url = artifact["url"].AsString();
        if (url.empty()) {
            const std::string base = library["url"].AsString();
            if (!base.empty()) url = JoinUrl(base, relative);
        }
        std::string sha1 = artifact["sha1"].AsString();
        if (sha1.empty()) sha1 = library["sha1"].AsString();

        const std::string dest = tools::JoinPath(game_libraries, relative);
        if (url.empty()) {
            // 没有下载地址的依赖只能靠本地已有文件
            if (!tools::PathExists(dest)) {
                error = "Fabric 依赖缺少下载地址: " + relative;
                return false;
            }
        } else {
            download::installer::Transfer::Task task;
            task.url = url;
            task.sha1 = sha1;
            task.dest = dest;
            task.label = relative;
            library_tasks.push_back(std::move(task));
        }
        classpath.push_back(relative);
    }

    if (!library_tasks.empty()) {
        download::installer::Transfer::Result library_result;
        if (!transfer_.Run(library_tasks, progress, library_result, error)) {
            error = "下载 Fabric 依赖失败: " + error;
            return false;
        }
    }

    if (!WriteServerMarker(versions_, id, name, "Fabric", mc_version, java_major, main_class,
                           classpath, error)) {
        return false;
    }
    version_id = id;
    return true;
}

bool Installer::Prepare(const std::string& id, const std::string& name, const std::string& loader,
                        const std::string& mc_version, int java_major, std::string& error) const {
    (void)name;
    (void)loader;
    (void)mc_version;
    (void)java_major;

    const std::string dir = versions_.VersionDir(id);
    std::string dir_error;
    if (!tools::EnsureDir(dir, dir_error)) {
        error = "创建服务端版本目录失败: " + dir + "（" + dir_error + "）";
        return false;
    }

    // 目录里已经有别的版本（客户端/外部放入的版本）时不覆盖
    const std::string marker_path = tools::JoinPath(dir, "rml.json");
    if (tools::PathExists(marker_path)) {
        Json::Value marker;
        Json parser;
        std::string parse_error;
        const std::string text = tools::ReadTextFile(marker_path);
        if (text.empty() || !parser.Parse(text, marker, parse_error) ||
            marker["kind"].AsString() != "server") {
            error = "版本目录被占用: " + dir + "（已有非服务端版本）";
            return false;
        }
    }
    return true;
}

bool Installer::WriteMarker(const std::string& id, const std::string& name,
                            const std::string& loader, const std::string& mc_version,
                            int java_major, std::string& error) const {
    return WriteServerMarker(versions_, id, name, loader, mc_version, java_major, std::string(),
                             {}, error);
}

}  // namespace server
