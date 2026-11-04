#include "app/bridge/Mods.h"

#include <QVariantMap>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

#include "app/bridge/Labels.h"
#include "app/directory/Installer/Placement/Placement.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace {

constexpr const char* kManifestName = "mods.json";

QString FromStd(const std::string& text) { return QString::fromStdString(text); }

bool ParseJsonText(const std::string& text, Json::Value& out) {
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
        Json::Value meta;
        std::string meta_text;
        // 仓库统一读 mods/rml.json；缺少记录的 jar 不参与管理。
        if (!paths.ReadRepositoryModText(file, meta_text) || !ParseJsonText(meta_text, meta)) {
            continue;
        }

        const QStringList game_versions = JsonStringList(meta["gameVersions"]);
        const QStringList loaders = JsonStringList(meta["loaders"]);
        const bool mc_ok = !mc_version.empty() && game_versions.contains(FromStd(mc_version));
        const bool loader_ok = !loader.empty() && loaders.contains(FromStd(loader));
        if (!mc_ok || !loader_ok) continue;

        QVariantMap info;
        info["file"] = FromStd(file);
        info["path"] = FromStd(tools::JoinPath(repo, file));
        info["known"] = true;
        info["compatible"] = true;
        info["reason"] = QString();
        info["project"] = FromStd(meta["project"].AsString());
        info["name"] = FromStd(meta["name"].AsString());
        info["version"] = FromStd(meta["version"].AsString());
        info["sha1"] = FromStd(meta["sha1"].AsString());
        info["gameVersions"] = game_versions;
        info["loaders"] = loaders;
        repository_items_.append(info);
    }
    return repository_items_;
}

QVariantList BridgeMods::Installed(const std::string& game_dir, const Directory::VersionEntry& entry) {
    installed_items_.clear();

    const Directory paths(game_dir);
    const std::string mods_dir = paths.InstalledModsRoot(entry.id);
    Json::Value manifest;
    std::string manifest_text;
    if (paths.ReadVersionText(entry.id, kManifestName, manifest_text) &&
        ParseJsonText(manifest_text, manifest) && manifest["mods"].IsArray()) {
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
        }
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
    // 优先硬链接，仓库和实例不占两份空间；失败再复制。
    std::filesystem::remove(tools::ToPath(dest), ec);
    ec.clear();
    std::filesystem::create_hard_link(tools::ToPath(source), tools::ToPath(dest), ec);
    if (ec) {
        ec.clear();
        std::filesystem::copy_file(tools::ToPath(source), tools::ToPath(dest),
                                   std::filesystem::copy_options::overwrite_existing, ec);
    }
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
    std::string manifest_text;
    if (!paths.ReadVersionText(entry.id, kManifestName, manifest_text) ||
        !ParseJsonText(manifest_text, manifest)) {
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
    if (!paths.WriteVersionText(entry.id, kManifestName, manifest.Dump())) {
        message = QStringLiteral("写入版本 Mod 清单失败");
        return false;
    }

    message = QStringLiteral("已安装到 %1：%2").arg(FromStd(entry.name)).arg(FromStd(file));
    return true;
}

bool BridgeMods::Import(const std::string& game_dir, const std::string& source,
                        const std::string& game_version, const std::string& loader,
                        const std::string& mod_version, QString& message) {
    if (game_version.empty() || loader.empty() || mod_version.empty()) {
        message = QStringLiteral("游戏版本、加载器和 Mod 版本都不能为空");
        return false;
    }
    const std::string loader_id = BridgeLabels::ContentLoaderId(loader, "");
    if (loader_id.empty()) {
        message = QStringLiteral("不认识的加载器：%1").arg(FromStd(loader));
        return false;
    }

    const std::filesystem::path src = tools::ToPath(source);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(src, ec)) {
        message = QStringLiteral("找不到要导入的文件：%1").arg(FromStd(source));
        return false;
    }
    const auto file_name = src.filename().u8string();
    const std::string source_file(file_name.begin(), file_name.end());
    if (!EndsWithJar(source_file)) {
        message = QStringLiteral("只能导入 .jar 模组文件");
        return false;
    }

    Directory paths(game_dir);
    std::string dir_error;
    if (!tools::EnsureDir(paths.RepositoryModsRoot(), dir_error)) {
        message = FromStd(dir_error);
        return false;
    }

    const std::string dest = directory::installer::Placement::Resolve(
        game_dir, directory::installer::Placement::Kind::Mod, source_file, false);
    const auto dest_name_u8 = tools::ToPath(dest).filename().u8string();
    const std::string dest_name(dest_name_u8.begin(), dest_name_u8.end());

    std::filesystem::copy_file(src, tools::ToPath(dest), ec);
    if (ec) {
        message = QStringLiteral("复制到仓库失败：%1").arg(FromStd(dest_name));
        return false;
    }

    std::string sha1;
    if (!tools::Sha1File(dest, sha1)) {
        std::error_code ignored;
        std::filesystem::remove(tools::ToPath(dest), ignored);
        message = QStringLiteral("计算 Mod 校验值失败");
        return false;
    }

    const size_t dot = dest_name.find_last_of('.');
    const std::string display_name = dot == std::string::npos ? dest_name : dest_name.substr(0, dot);

    Json::Value meta = Json::Value::Object();
    meta.Set("file", Json::Value::String(dest_name));
    meta.Set("project", Json::Value::String(""));
    meta.Set("name", Json::Value::String(display_name));
    meta.Set("version", Json::Value::String(mod_version));
    meta.Set("side", Json::Value::String("both"));
    meta.Set("sha1", Json::Value::String(sha1));
    Json::Value game_versions = Json::Value::Array();
    game_versions.Push(Json::Value::String(game_version));
    meta.Set("gameVersions", game_versions);
    Json::Value loaders = Json::Value::Array();
    loaders.Push(Json::Value::String(loader_id));
    meta.Set("loaders", loaders);

    if (!paths.WriteRepositoryModText(dest_name, meta.Dump())) {
        std::error_code ignored;
        std::filesystem::remove(tools::ToPath(dest), ignored);
        message = QStringLiteral("写入 Mod 版本描述失败");
        return false;
    }

    message = QStringLiteral("已导入 Mod 仓库：%1").arg(FromStd(dest_name));
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
    std::string manifest_text;
    if (paths.ReadVersionText(entry.id, kManifestName, manifest_text) &&
        ParseJsonText(manifest_text, manifest) && manifest["mods"].IsArray()) {
        Json::Value next = Json::Value::Array();
        for (size_t i = 0; i < manifest["mods"].Size(); ++i) {
            if (manifest["mods"].At(i)["file"].AsString() != file) {
                next.Push(manifest["mods"].At(i));
            }
        }
        manifest.Set("mods", next);
        paths.WriteVersionText(entry.id, kManifestName, manifest.Dump());
    }

    message = QStringLiteral("已从版本移除：%1").arg(FromStd(file));
    return true;
}


