#include "app/launch/Version/Version.h"

#include <algorithm>

#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace launch {

namespace {

std::string ArtifactPath(const Json::Value& artifact) {
    std::string path = artifact["path"].AsString();
    return path;
}

// 父版本和子版本的 arguments 都要保留（Forge 只在自己的 json 里补 --launchTarget 这类参数）
Json::Value MergeArguments(const Json::Value& parent, const Json::Value& child) {
    Json::Value out = Json::Value::Object();
    for (const char* key : {"game", "jvm"}) {
        Json::Value merged = Json::Value::Array();
        const Json::Value& from_parent = parent[key];
        const Json::Value& from_child = child[key];
        for (size_t i = 0; i < from_parent.Size(); ++i) merged.Push(from_parent.At(i));
        for (size_t i = 0; i < from_child.Size(); ++i) merged.Push(from_child.At(i));
        if (merged.Size() > 0) out.Set(key, merged);
    }
    for (size_t i = 0; i < child.Size(); ++i) {
        const std::string& key = child.KeyAt(i);
        if (key == "game" || key == "jvm") continue;
        out.Set(key, child.At(i));
    }
    return out;
}

}  // namespace

Version::Version(std::string game_dir) : paths_(std::move(game_dir)) { Refresh(); }

std::string Version::VersionDir(const std::string& id) const { return paths_.VersionDir(id); }

bool Version::Select(const std::string& id) {
    for (const auto& entry : Entries()) {
        if (entry.id == id) {
            selected_ = id;
            return true;
        }
    }

    // 原版底座被 Forge/NeoForge 接管时，选中底座等价于选中对应的加载器版本
    const std::string owner = paths_.DependencyOwner(id);
    if (!owner.empty()) {
        for (const auto& entry : Entries()) {
            if (entry.id == owner) {
                selected_ = owner;
                return true;
            }
        }
    }
    return false;
}

bool Version::Rename(const std::string& id, const std::string& name) {
    const std::string dir = VersionDir(id);
    if (!tools::IsDirectory(dir)) return false;

    const std::string marker_path = tools::JoinPath(dir, "rml.json");
    Json::Value root = Json::Value::Object();
    Json json;
    std::string error;
    const std::string existing = tools::ReadTextFile(marker_path);
    if (!existing.empty() && !json.Parse(existing, root, error)) return false;

    root.Set("name", Json::Value::String(name));
    if (!tools::WriteTextFile(marker_path, root.Dump())) return false;

    paths_.RefreshVersions();
    Select(id);
    return true;
}

void Version::Refresh() {
    const std::string previous = selected_;
    paths_.RefreshVersions();
    if (!previous.empty()) {
        if (!Select(previous)) selected_.clear();
    }
}

bool Version::LoadOne(const std::string& id, Detail& out, std::string& error, int depth) const {
    if (depth > 8) {
        error = "继承链过深: " + id;
        return false;
    }

    const std::string json_path = paths_.VersionJson(id);
    const std::string text = tools::ReadTextFile(json_path);
    if (text.empty()) {
        error = "读不到版本描述: " + json_path;
        return false;
    }

    Json::Value root;
    Json json;
    if (!json.Parse(text, root, error)) {
        error = json_path + ": " + error;
        return false;
    }

    Detail detail;
    detail.id = root["id"].AsString().empty() ? id : root["id"].AsString();
    detail.main_class = root["mainClass"].AsString();
    detail.assets = root["assets"].AsString();

    const Json::Value& asset_index = root["assetIndex"];
    if (asset_index.IsObject()) {
        detail.asset_index = asset_index["id"].AsString();
        if (detail.asset_index.empty()) detail.asset_index = detail.assets;
        detail.asset_index_url = asset_index["url"].AsString();
        detail.asset_index_sha1 = asset_index["sha1"].AsString();
    }

    const Json::Value& client_download = root["downloads"]["client"];
    detail.client_url = client_download["url"].AsString();
    detail.client_sha1 = client_download["sha1"].AsString();
    detail.client_size = client_download["size"].AsInt(0);

    if (root["javaVersion"].IsObject()) {
        detail.java_major = static_cast<int>(root["javaVersion"]["majorVersion"].AsInt(8));
    }

    if (root["arguments"].IsObject()) {
        detail.arguments = root["arguments"];
        detail.has_arguments = true;
    }
    detail.legacy_arguments = root["minecraftArguments"].AsString();

    const std::string own_jar = paths_.VersionJar(id);
    if (tools::PathExists(own_jar)) detail.client_jar = own_jar;

    const Json::Value& libraries = root["libraries"];
    for (size_t i = 0; i < libraries.Size(); ++i) {
        const Json::Value& item = libraries.At(i);
        Library library;
        library.name = item["name"].AsString();
        library.path = ArtifactPath(item["downloads"]["artifact"]);
        library.url = item["downloads"]["artifact"]["url"].AsString();
        library.sha1 = item["downloads"]["artifact"]["sha1"].AsString();
        library.size = item["downloads"]["artifact"]["size"].AsInt(0);
        library.rules = item["rules"];

        // 老格式和 Fabric meta：没有 downloads.artifact，路径要从 maven 名字推；
        // item["url"] 此时是仓库根（Fabric 的 https://maven.fabricmc.net/），要拼上相对路径。
        if (library.path.empty() && !library.name.empty() && !item["natives"].IsObject()) {
            library.path = tools::MavenToPath(library.name);
            if (!library.path.empty() && item["url"].IsString() &&
                !item["url"].AsString().empty()) {
                std::string base = item["url"].AsString();
                if (!base.empty() && base.back() != '/') base += "/";
                library.url = base + library.path;
            } else if (library.url.empty() && !library.path.empty()) {
                library.url = "https://libraries.minecraft.net/" + library.path;
            }
        }

        const Json::Value& natives = item["natives"];
        for (size_t k = 0; k < natives.Size(); ++k) {
            if (natives.At(k).IsString()) {
                library.natives[natives.KeyAt(k)] = natives.At(k).AsString();
            }
        }
        const Json::Value& classifiers = item["downloads"]["classifiers"];
        for (size_t k = 0; k < classifiers.Size(); ++k) {
            const Json::Value& artifact = classifiers.At(k);
            Classifier classifier;
            classifier.path = ArtifactPath(artifact);
            classifier.url = artifact["url"].AsString();
            classifier.sha1 = artifact["sha1"].AsString();
            classifier.size = artifact["size"].AsInt(0);
            library.classifiers[classifiers.KeyAt(k)] = std::move(classifier);
        }
        if (!library.name.empty()) detail.libraries.push_back(std::move(library));
    }

    const std::string parent_id = root["inheritsFrom"].AsString();
    if (!parent_id.empty()) {
        Detail parent;
        if (!LoadOne(parent_id, parent, error, depth + 1)) return false;

        // 子版本（Forge/NeoForge）的库排在前面：它们自带的 log4j/ASM 等新版本
        // 必须覆盖父版本里的旧版本，否则 modlauncher 会在启动阶段 NoSuchMethodError。
        std::vector<Library> merged = detail.libraries;
        merged.insert(merged.end(), parent.libraries.begin(), parent.libraries.end());
        detail.libraries = std::move(merged);

        if (detail.main_class.empty()) detail.main_class = parent.main_class;
        if (detail.assets.empty()) detail.assets = parent.assets;
        if (detail.asset_index.empty()) detail.asset_index = parent.asset_index;
        if (!root["javaVersion"].IsObject()) detail.java_major = parent.java_major;
        if (detail.client_jar.empty()) detail.client_jar = parent.client_jar;
        if (detail.client_url.empty()) {
            detail.client_url = parent.client_url;
            detail.client_sha1 = parent.client_sha1;
            detail.client_size = parent.client_size;
        }
        if (detail.asset_index_url.empty()) {
            detail.asset_index_url = parent.asset_index_url;
            detail.asset_index_sha1 = parent.asset_index_sha1;
        }
        if (!detail.has_arguments) {
            detail.arguments = parent.arguments;
            detail.has_arguments = parent.has_arguments;
            // 子版本自己的 minecraftArguments（Forge 的 --tweakClass）不能被父版本覆盖
            if (detail.legacy_arguments.empty()) detail.legacy_arguments = parent.legacy_arguments;
        } else if (parent.has_arguments) {
            detail.arguments = MergeArguments(parent.arguments, detail.arguments);
        }
    }

    out = std::move(detail);
    return true;
}

bool Version::LoadDetail(Detail& out, std::string& error) const {
    if (selected_.empty()) {
        error = "没有选中的版本";
        return false;
    }
    return LoadOne(selected_, out, error, 0);
}

}  // namespace launch
