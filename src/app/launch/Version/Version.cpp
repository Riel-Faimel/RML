#include "app/launch/Version/Version.h"

#include <algorithm>

#include "app/tools/tools.h"

namespace launch {

namespace {

Json::Value ParseArtifact(const Json::Value& value) {
    return value;
}

std::string ArtifactPath(const Json::Value& artifact) {
    std::string path = artifact["path"].AsString();
    return path;
}

}  // namespace

Version::Version(std::string game_dir) : game_dir_(std::move(game_dir)) { Probe(); }

std::string Version::VersionDir(const std::string& id) const {
    return tools::JoinPath(tools::JoinPath(game_dir_, "versions"), id);
}

void Version::Probe() {
    entries_.clear();
    const std::string versions_dir = tools::JoinPath(game_dir_, "versions");
    if (!tools::IsDirectory(versions_dir)) return;

    for (const auto& id : tools::ListDirectories(versions_dir)) {
        Entry entry;
        entry.id = id;
        entry.directory = tools::JoinPath(versions_dir, id);
        entry.has_json = tools::PathExists(tools::JoinPath(entry.directory, id + ".json"));
        entry.has_jar = tools::PathExists(tools::JoinPath(entry.directory, id + ".jar"));
        entries_.push_back(std::move(entry));
    }

    std::sort(entries_.begin(), entries_.end(),
              [](const Entry& a, const Entry& b) { return a.id < b.id; });
}

bool Version::Select(const std::string& id) {
    for (const auto& entry : entries_) {
        if (entry.id == id) {
            selected_ = id;
            return true;
        }
    }
    return false;
}

void Version::Refresh() {
    const std::string previous = selected_;
    Probe();
    if (!previous.empty()) {
        if (!Select(previous)) selected_.clear();
    }
}

bool Version::LoadOne(const std::string& id, Detail& out, std::string& error, int depth) const {
    if (depth > 8) {
        error = "继承链过深: " + id;
        return false;
    }

    const std::string dir = VersionDir(id);
    const std::string json_path = tools::JoinPath(dir, id + ".json");
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

    const std::string own_jar = tools::JoinPath(dir, id + ".jar");
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

        const Json::Value& natives = item["natives"];
        for (size_t k = 0; k < natives.Size(); ++k) {
            if (natives.At(k).IsString()) {
                library.natives[natives.KeyAt(k)] = natives.At(k).AsString();
            }
        }
        const Json::Value& classifiers = item["downloads"]["classifiers"];
        for (size_t k = 0; k < classifiers.Size(); ++k) {
            library.classifier_paths[classifiers.KeyAt(k)] =
                ArtifactPath(classifiers.At(k));
        }
        if (!library.name.empty()) detail.libraries.push_back(std::move(library));
    }

    const std::string parent_id = root["inheritsFrom"].AsString();
    if (!parent_id.empty()) {
        Detail parent;
        if (!LoadOne(parent_id, parent, error, depth + 1)) return false;

        // 父版本的库排在前面，子版本追加；其余字段子版本优先
        std::vector<Library> merged = parent.libraries;
        merged.insert(merged.end(), detail.libraries.begin(), detail.libraries.end());
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
            detail.legacy_arguments = parent.legacy_arguments;
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
