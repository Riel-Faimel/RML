#include "app/bridge/Mods.h"

#include <QVariantMap>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

#include "app/bridge/Labels.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace {

QString FromStd(const std::string& text) { return QString::fromStdString(text); }

bool ReadJsonFile(const std::string& path, Json::Value& out) {
    const std::string text = tools::ReadTextFile(path);
    if (text.empty()) return false;
    Json json;
    std::string error;
    return json.Parse(text, out, error);
}

QStringList JsonStringList(const Json::Value& value) {
    QStringList out;
    if (!value.IsArray()) return out;
    for (size_t i = 0; i < value.Size(); ++i) out << FromStd(value.At(i).AsString());
    return out;
}

bool EndsWithJar(const std::string& path) {
    if (path.size() < 4) return false;
    std::string suffix = path.substr(path.size() - 4);
    for (char& c : suffix) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return suffix == ".jar";
}

}  // namespace

QVariantList BridgeMods::Repository(const std::string& game_dir, const Directory::VersionEntry& entry) {
    repository_items_.clear();

    const std::string mc_version = entry.minecraft_version;
    const std::string loader = BridgeLabels::ContentLoaderId(entry.loader, entry.id);
    const Directory paths(game_dir);
    const std::string repo = paths.RepositoryModsRoot();

    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& item : std::filesystem::directory_iterator(tools::ToPath(repo), ec)) {
        if (ec || !item.is_regular_file()) continue;
        const auto name = item.path().filename().u8string();
        const std::string file(name.begin(), name.end());
        if (EndsWithJar(file)) files.push_back(file);
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        QVariantMap info;
        info["file"] = FromStd(file);
        info["name"] = FromStd(file);
        info["version"] = QString();
        info["project"] = QString();
        info["path"] = FromStd(tools::JoinPath(repo, file));
        info["known"] = false;
        info["compatible"] = false;
        info["reason"] = QStringLiteral("缺少版本描述，请重新下载");

        Json::Value meta;
        if (ReadJsonFile(info["path"].toString().toStdString() + ".rml.json", meta)) {
            const QStringList game_versions = JsonStringList(meta["gameVersions"]);
            const QStringList loaders = JsonStringList(meta["loaders"]);
            const bool mc_ok = !mc_version.empty() && game_versions.contains(FromStd(mc_version));
            const bool loader_ok = !loader.empty() && loaders.contains(FromStd(loader));
            info["known"] = true;
            info["project"] = FromStd(meta["project"].AsString());
            info["name"] = FromStd(meta["name"].AsString());
            info["version"] = FromStd(meta["version"].AsString());
            info["sha1"] = FromStd(meta["sha1"].AsString());
            info["gameVersions"] = game_versions;
            info["loaders"] = loaders;
            info["compatible"] = mc_ok && loader_ok;
            info["reason"] = info["compatible"].toBool()
                                 ? QString()
                                 : QStringLiteral("不适用于当前版本或加载器");
        }
        repository_items_.append(info);
    }
    return repository_items_;
}

QVariantList BridgeMods::Installed(const std::string& game_dir, const Directory::VersionEntry& entry) {
    installed_items_.clear();

    const Directory paths(game_dir);
    const std::string mods_dir = paths.InstalledModsRoot(entry.id);
    const std::string manifest_path = paths.InstalledManifest(entry.id);

    std::vector<std::string> listed;
    Json::Value manifest;
    if (ReadJsonFile(manifest_path, manifest) && manifest["mods"].IsArray()) {
        for (size_t i = 0; i < manifest["mods"].Size(); ++i) {
            const Json::Value& item = manifest["mods"].At(i);
            const std::string file = item["file"].AsString();
            if (file.empty() || !tools::PathExists(tools::JoinPath(mods_dir, file))) continue;
            QVariantMap info;
            info["file"] = FromStd(file);
            info["name"] = FromStd(item["name"].AsString());
            info["version"] = FromStd(item["version"].AsString());
            info["project"] = FromStd(item["project"].AsString());
            info["path"] = FromStd(tools::JoinPath(mods_dir, file));
            info["known"] = true;
            installed_items_.append(info);
            listed.push_back(file);
        }
    }

    // 用户手动放进版本 mods 的文件也允许在列表里删除
    std::error_code ec;
    for (const auto& item : std::filesystem::directory_iterator(tools::ToPath(mods_dir), ec)) {
        if (ec || !item.is_regular_file()) continue;
        const auto name = item.path().filename().u8string();
        const std::string file(name.begin(), name.end());
        if (!EndsWithJar(file) || std::find(listed.begin(), listed.end(), file) != listed.end()) {
            continue;
        }
        QVariantMap info;
        info["file"] = FromStd(file);
        info["name"] = FromStd(file);
        info["version"] = QString();
        info["project"] = QString();
        info["path"] = FromStd(tools::JoinPath(mods_dir, file));
        info["known"] = false;
        installed_items_.append(info);
    }
    return installed_items_;
}

bool BridgeMods::Install(const std::string& game_dir, const Directory::VersionEntry& entry, int index,
                   QString& message) {
    if (index < 0 || index >= repository_items_.size()) return false;
    const QVariantMap item = repository_items_.at(index).toMap();
    if (!item["compatible"].toBool()) {
        message = QStringLiteral("无法安装：%1").arg(item["reason"].toString());
        return false;
    }

    const Directory paths(game_dir);
    std::string error;
    if (!paths.EnsureInstalledModsRoot(entry.id, error)) {
        message = FromStd(error);
        return false;
    }

    const std::string file = item["file"].toString().toStdString();
    const std::string source = item["path"].toString().toStdString();
    const std::string dest = tools::JoinPath(paths.InstalledModsRoot(entry.id), file);
    std::error_code ec;
    std::filesystem::copy_file(tools::ToPath(source), tools::ToPath(dest),
                               std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        message = QStringLiteral("安装到版本失败：%1").arg(FromStd(file));
        return false;
    }

    // 清理旧逻辑产生的 (1)/(2) 副本
    const size_t dot = file.find_last_of('.');
    if (dot != std::string::npos) {
        const std::string stem = file.substr(0, dot);
        const std::string ext = file.substr(dot);
        for (int n = 1; n <= 20; ++n) {
            std::filesystem::remove(
                tools::ToPath(tools::JoinPath(paths.InstalledModsRoot(entry.id),
                                              stem + " (" + std::to_string(n) + ")" + ext)),
                ec);
        }
    }

    Json::Value manifest;
    if (!ReadJsonFile(paths.InstalledManifest(entry.id), manifest)) {
        manifest = Json::Value::Object();
    }
    Json::Value mods = manifest["mods"].IsArray() ? manifest["mods"] : Json::Value::Array();
    Json::Value next = Json::Value::Array();
    for (size_t i = 0; i < mods.Size(); ++i) {
        if (mods.At(i)["file"].AsString() != file) next.Push(mods.At(i));
    }
    Json::Value record = Json::Value::Object();
    record.Set("file", Json::Value::String(file));
    record.Set("project", Json::Value::String(item["project"].toString().toStdString()));
    record.Set("name", Json::Value::String(item["name"].toString().toStdString()));
    record.Set("version", Json::Value::String(item["version"].toString().toStdString()));
    record.Set("sha1", Json::Value::String(item["sha1"].toString().toStdString()));
    Json::Value game_versions = Json::Value::Array();
    for (const auto& value : item["gameVersions"].toStringList()) {
        game_versions.Push(Json::Value::String(value.toStdString()));
    }
    Json::Value loaders = Json::Value::Array();
    for (const auto& value : item["loaders"].toStringList()) {
        loaders.Push(Json::Value::String(value.toStdString()));
    }
    record.Set("gameVersions", game_versions);
    record.Set("loaders", loaders);
    next.Push(record);
    manifest.Set("mods", next);
    if (!tools::WriteTextFile(paths.InstalledManifest(entry.id), manifest.Dump())) {
        message = QStringLiteral("写入版本 Mod 清单失败");
        return false;
    }

    message = QStringLiteral("已安装到 %1：%2").arg(FromStd(entry.name)).arg(FromStd(file));
    return true;
}

bool BridgeMods::Remove(const std::string& game_dir, const Directory::VersionEntry& entry, int index,
                  QString& message) {
    if (index < 0 || index >= installed_items_.size()) return false;
    const QVariantMap item = installed_items_.at(index).toMap();
    const std::string file = item["file"].toString().toStdString();

    const Directory paths(game_dir);
    std::error_code ec;
    std::filesystem::remove(tools::ToPath(tools::JoinPath(paths.InstalledModsRoot(entry.id), file)),
                            ec);

    Json::Value manifest;
    if (ReadJsonFile(paths.InstalledManifest(entry.id), manifest) && manifest["mods"].IsArray()) {
        Json::Value next = Json::Value::Array();
        for (size_t i = 0; i < manifest["mods"].Size(); ++i) {
            if (manifest["mods"].At(i)["file"].AsString() != file) {
                next.Push(manifest["mods"].At(i));
            }
        }
        manifest.Set("mods", next);
        tools::WriteTextFile(paths.InstalledManifest(entry.id), manifest.Dump());
    }

    message = QStringLiteral("已从版本移除：%1").arg(FromStd(file));
    return true;
}


