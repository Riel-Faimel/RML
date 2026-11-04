#include "app/bridge/Content.h"

#include <QMetaObject>

#include <thread>
#include <vector>

#include "app/directory/Directory.h"
#include "app/download/Installer/Placement/Placement.h"
#include "app/download/Installer/Transfer/Transfer.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace {

QString FromStd(const std::string& text) { return QString::fromStdString(text); }

bool ContainsValue(const std::vector<std::string>& values, const std::string& wanted) {
    for (const auto& value : values) {
        if (value == wanted) return true;
    }
    return false;
}

}  // namespace

BridgeContent::BridgeContent(Launch& launch, NetTask& tasks, QObject* context,
                             BridgeDownloadState& state, Notify download_changed)
    : launch_(launch),
      tasks_(tasks),
      context_(context),
      state_(state),
      download_changed_(std::move(download_changed)) {
    // 内容源：Modrinth / CurseForge（缺 key 的会标注出来）
    for (auto* source : launch_.Downloader().Sources().Sources()) {
        std::string reason;
        const bool ready = source->Ready(reason);
        sources_ << QStringLiteral("%1%2")
                        .arg(FromStd(source->DisplayName()))
                        .arg(ready ? QString() : QStringLiteral("（%1）").arg(FromStd(reason)));
    }
}

ContentSource* BridgeContent::SelectedSource() {
    auto sources = launch_.Downloader().Sources().Sources();
    if (source_index_ < 0 || static_cast<size_t>(source_index_) >= sources.size()) {
        return sources.empty() ? nullptr : sources.front();
    }
    return sources[static_cast<size_t>(source_index_)];
}

void BridgeContent::SelectSource(int index) {
    source_index_ = index;
    download_changed_();
}

void BridgeContent::Search(const QString& query, const QString& kind, const std::string& mc_version,
                     const std::string& loader) {
    if (state_.busy) return;

    if (mc_version.empty()) {
        state_.status = QStringLiteral("先选择一个游戏版本，再搜索内容");
        download_changed_();
        return;
    }
    if (kind == QStringLiteral("mod") && loader.empty()) {
        state_.status = QStringLiteral("当前版本没有 Mod 加载器，不能安装 Mod");
        download_changed_();
        return;
    }

    kind_ = kind;
    state_.busy = true;
    state_.failed = false;
    state_.status = QStringLiteral("正在搜索…");
    download_changed_();

    const std::string text = query.toStdString();
    std::thread([this, text, loader, mc_version] {
        std::vector<ContentSource::Project> projects;
        std::string error;
        bool ok = SelectedSource()->Search(text, loader, mc_version, projects, error);

        QStringList titles;
        QStringList names;
        QStringList ids;
        if (ok) {
            for (const auto& project : projects) {
                titles << QStringLiteral("%1  ·  %2").arg(FromStd(project.title)).arg(FromStd(project.slug));
                names << FromStd(project.title);
                ids << FromStd(project.id);
            }
        }
        QMetaObject::invokeMethod(
            context_,
            [this, ok, titles, names, ids, error] {
                results_ = titles;
                names_ = names;
                ids_ = ids;
                state_.status = ok ? QStringLiteral("找到 %1 个结果").arg(titles.size())
                                   : QStringLiteral("搜索失败：%1").arg(FromStd(error));
                state_.busy = false;
                state_.failed = !ok;
                download_changed_();
            },
            Qt::QueuedConnection);
    }).detach();
}

void BridgeContent::Install(int index, const std::string& mc_version, const std::string& loader,
                      const std::string& instance_dir) {
    if (state_.busy || index < 0 || index >= ids_.size()) return;

    const std::string project_id = ids_.at(index).toStdString();
    const std::string project_name = names_.value(index).toStdString();
    const std::string kind = kind_.toStdString();
    if (mc_version.empty()) {
        state_.status = QStringLiteral("先选择一个游戏版本，再安装内容");
        download_changed_();
        return;
    }
    if (kind == "mod" && loader.empty()) {
        state_.status = QStringLiteral("当前版本没有 Mod 加载器，不能安装 Mod");
        download_changed_();
        return;
    }

    state_.busy = true;
    state_.failed = false;
    state_.progress = 0.0;
    launch_.Downloader().Installers().Transfers().ResetCancel();
    state_.status = QStringLiteral("正在查找适配 %1 的版本…").arg(FromStd(mc_version));
    download_changed_();

    std::thread([this, project_id, project_name, kind, instance_dir, loader, mc_version] {
        std::vector<ContentSource::Version> versions;
        std::string error;

        if (!SelectedSource()->Versions(project_id, loader, mc_version, versions, error)) {
            QMetaObject::invokeMethod(
                context_,
                [this, error] {
                    state_.status = QStringLiteral("解析失败：%1").arg(FromStd(error));
                    state_.busy = false;
                    state_.failed = true;
                    download_changed_();
                },
                Qt::QueuedConnection);
            return;
        }

        const ContentSource::Version* chosen = nullptr;
        for (const auto& version : versions) {
            if (version.files.empty()) continue;
            if (!version.game_versions.empty() &&
                !ContainsValue(version.game_versions, mc_version)) {
                continue;
            }
            if (kind == "mod" && !version.loaders.empty() &&
                !ContainsValue(version.loaders, loader)) {
                continue;
            }
            chosen = &version;
            break;
        }

        if (chosen == nullptr) {
            const QString reason = kind == "mod" && !loader.empty()
                                       ? QStringLiteral("没有适配 %1（%2）的版本")
                                             .arg(FromStd(mc_version))
                                             .arg(FromStd(loader))
                                       : QStringLiteral("没有适配 %1 的版本").arg(FromStd(mc_version));
            QMetaObject::invokeMethod(
                context_,
                [this, reason] {
                    state_.status = reason;
                    state_.busy = false;
                    state_.failed = true;
                    download_changed_();
                },
                Qt::QueuedConnection);
            return;
        }

        const auto& file = chosen->files.front();
        download::installer::Placement::Kind placement_kind = download::installer::Placement::Kind::Mod;
        if (kind == "shader") placement_kind = download::installer::Placement::Kind::Shaderpack;
        if (kind == "resourcepack") placement_kind = download::installer::Placement::Kind::Resourcepack;

        NetTask::Resource resource;
        resource.url = file.url;
        resource.sha1 = file.sha1;
        resource.dest = download::installer::Placement::Resolve(instance_dir, placement_kind,
                                                                file.filename, true);
        resource.label = file.filename;
        resource.size = file.size;

        const std::string version_name =
            chosen->name.empty() ? chosen->version_number : chosen->name;
        const std::vector<std::string> game_versions = chosen->game_versions;
        const std::vector<std::string> loaders = chosen->loaders;
        tasks_.Submit("下载 " + (project_name.empty() ? file.filename : project_name),
                      {resource},
                      [this, kind, project_id, project_name, version_name, game_versions, loaders,
                       instance_dir, dest = resource.dest, filename = file.filename,
                       sha1 = file.sha1](bool ok, const std::string& download_error) {
                          std::string error = download_error;
                          if (ok && kind == "mod") {
                              Json::Value meta = Json::Value::Object();
                              meta.Set("file", Json::Value::String(filename));
                              meta.Set("project", Json::Value::String(project_id));
                              meta.Set("name", Json::Value::String(project_name.empty() ? project_id
                                                                                        : project_name));
                              meta.Set("version", Json::Value::String(version_name));
                              meta.Set("side", Json::Value::String("both"));
                              meta.Set("sha1", Json::Value::String(sha1));
                              Json::Value game_versions_json = Json::Value::Array();
                              for (const auto& value : game_versions) {
                                  game_versions_json.Push(Json::Value::String(value));
                              }
                              Json::Value loaders_json = Json::Value::Array();
                              for (const auto& value : loaders) {
                                  loaders_json.Push(Json::Value::String(value));
                              }
                              meta.Set("gameVersions", game_versions_json);
                              meta.Set("loaders", loaders_json);
                              if (!Directory(instance_dir).WriteRepositoryModText(
                                      filename, meta.Dump())) {
                                  ok = false;
                                  error = "写入 Mod 仓库描述失败: " + dest;
                              }
                          }
                          QMetaObject::invokeMethod(
                              context_,
                              [this, ok, error, filename] {
                                  state_.status =
                                      ok ? QStringLiteral("已下载到 Mod 仓库：%1").arg(FromStd(filename))
                                         : QStringLiteral("下载失败：%1").arg(FromStd(error));
                                  state_.busy = false;
                                  state_.failed = !ok;
                                  if (ok) state_.progress = 1.0;
                                  download_changed_();
                              },
                              Qt::QueuedConnection);
                      });
    }).detach();
}


