#include "app/Bridge.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QMetaObject>
#include <QUrl>

#include <filesystem>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace {

QString FromStd(const std::string& text) { return QString::fromStdString(text); }

// 从主类判断加载器，够用就行，不解析 mod 目录
QString LoaderFromMainClass(const std::string& main_class) {
    if (main_class.find("fabric") != std::string::npos) return QStringLiteral("Fabric");
    if (main_class.find("quilt") != std::string::npos) return QStringLiteral("Quilt");
    if (main_class.find("neoforge") != std::string::npos) return QStringLiteral("NeoForge");
    if (main_class.find("forge") != std::string::npos) return QStringLiteral("Forge");
    return QStringLiteral("原版");
}

}  // namespace

Bridge::Bridge(Launch& launch, QString account_name, int memory_mb, QObject* parent)
    : QObject(parent),
      launch_(launch),
      account_name_(std::move(account_name)),
      memory_mb_(memory_mb) {
    for (const auto& entry : launch_.Versions().Entries()) {
        versions_ << FromStd(entry.id);
    }

    if (!versions_.isEmpty()) {
        // 尊重 settings.json 里的选择；只有没有选择时才退到第一个
        const std::string saved = launch_.Versions().Selected();
        if (!saved.empty() && versions_.contains(FromStd(saved))) {
            selected_ = FromStd(saved);
        } else {
            selected_ = versions_.first();
            launch_.Versions().Select(selected_.toStdString());
        }
        UpdateDetails();
    }

    const auto& javas = launch_.Tools().Javas();
    (void)javas;
    RefreshJavas();
    if (javas_.isEmpty()) java_info_ = QStringLiteral("未发现 Java");

    // 内容源：Modrinth / CurseForge（缺 key 的会标注出来）
    for (auto* source : launch_.Downloader().Sources().Sources()) {
        std::string reason;
        const bool ready = source->Ready(reason);
        content_sources_ << QStringLiteral("%1%2")
                                .arg(FromStd(source->DisplayName()))
                                .arg(ready ? QString() : QStringLiteral("（%1）").arg(FromStd(reason)));
    }

    connect(&status_timer_, &QTimer::timeout, this, &Bridge::RefreshStatus);
    status_timer_.start(1000);
}

QString Bridge::GameDir() const { return FromStd(launch_.Versions().GameDir()); }

QString Bridge::BackgroundUrl() const {
    // 运行时可以换：把自己的图放到 %APPDATA%\RML\background.png 就覆盖默认背景
    const QString custom = FromStd(tools::DataRoot()) + QStringLiteral("/background.png");
    if (QFile::exists(custom)) return QUrl::fromLocalFile(custom).toString();

    // 部署后优先用 exe 旁边的 qml/assets
    const QString local = QCoreApplication::applicationDirPath() +
                          QStringLiteral("/qml/assets/background.png");
    if (QFile::exists(local)) return QUrl::fromLocalFile(local).toString();

    return QUrl::fromLocalFile(QStringLiteral(RML_QML_DIR "/assets/background.png")).toString();
}

void Bridge::selectVersion(const QString& id) {
    if (!launch_.Versions().Select(id.toStdString())) return;
    selected_ = id;
    UpdateDetails();
    emit changed();
}

void Bridge::UpdateDetails() {
    launch::Version::Detail detail;
    std::string error;
    if (!launch_.Versions().LoadDetail(detail, error)) {
        details_ = QStringLiteral("解析失败：%1").arg(FromStd(error));
        emit changed();
        return;
    }

    const auto& java = launch_.Tools().Pick(detail.java_major);
    QString java_line = java ? QStringLiteral("使用时 %1（Java %2）").arg(FromStd(java->path)).arg(java->major)
                             : QStringLiteral("没有可用 Java");

    loader_ = LoaderFromMainClass(detail.main_class);

    // 版本来源：RML 装的有 rml.json 标记，其余是外部放进来的
    QString origin = QStringLiteral("外部版本");
    for (const auto& entry : launch_.Versions().Entries()) {
        if (entry.id != selected_.toStdString()) continue;
        if (entry.managed) {
            origin = entry.loader.empty() ? QStringLiteral("RML 安装")
                                          : QStringLiteral("RML 安装 · %1").arg(FromStd(entry.loader));
            if (!entry.complete) origin += QStringLiteral("（未完成）");
        }
        break;
    }

    details_ = QStringLiteral("%1\n需要 Java %2\n库文件 %3 个\n客户端 jar %4")
                   .arg(origin)
                   .arg(detail.java_major)
                   .arg(detail.libraries.size())
                   .arg(detail.client_jar.empty() ? QStringLiteral("缺失") : QStringLiteral("就绪"));
    java_info_ = java_line;
    emit changed();
}

void Bridge::RefreshJavas() {
    javas_.clear();
    java_options_.clear();
    java_paths_.clear();

    java_options_ << QStringLiteral("自动（推荐）");

    for (const auto& java : launch_.Tools().Javas()) {
        const bool managed = java.source == "RML";
        const QString tag = managed ? QStringLiteral("RML 安装") : QStringLiteral("系统");

        // 设置页：来源标记放尾部
        javas_ << QStringLiteral("Java %1  ·  %2  ·  %3")
                      .arg(java.major)
                      .arg(FromStd(java.path))
                      .arg(tag);

        // 启动页下拉：短一点
        java_options_ << QStringLiteral("Java %1  ·  %2").arg(java.major).arg(tag);
        java_paths_ << FromStd(java.path);
    }
}

void Bridge::selectJava(int index) {
    if (index <= 0 || index > java_paths_.size()) {
        java_path_override_.clear();
    } else {
        java_path_override_ = java_paths_.at(index - 1);
    }
    UpdateDetails();
    emit changed();
}

void Bridge::installJava(int major) {
    if (busy_) return;
    busy_ = true;
    status_ = QStringLiteral("正在下载并安装 Java %1…").arg(major);
    emit changed();

    std::thread([this, major] {
        std::string error;
        const bool ok = launch_.Tools().Install(major, error);
        QMetaObject::invokeMethod(
            this,
            [this, ok, error, major] {
                if (ok) {
                    RefreshJavas();
                    UpdateDetails();
                    status_ = QStringLiteral("Java %1 已安装到 RML 的运行时目录").arg(major);
                } else {
                    status_ = QStringLiteral("安装失败：%1").arg(FromStd(error));
                }
                busy_ = false;
                emit changed();
            },
            Qt::QueuedConnection);
    }).detach();
}

void Bridge::launchGame() {
    if (busy_) return;

    AccountProvider::Credentials credentials;
    credentials.username = account_name_.toStdString();
    credentials.uuid = "00000000-0000-0000-0000-000000000000";
    credentials.access_token = "0";
    credentials.user_type = "legacy";

    launch::Starter::Options options;
    options.memory_mb = memory_mb_;
    options.java_path = java_path_override_.toStdString();
    launch::Starter::Command command;
    std::vector<launch::Starter::Missing> missing;
    std::string error;

    if (!launch_.GameStarter().Build(credentials, options, command, missing, error)) {
        QStringList lines;
        lines << QStringLiteral("无法启动：%1").arg(FromStd(error));
        for (const auto& item : missing) {
            lines << QStringLiteral("· %1（%2）").arg(FromStd(item.what)).arg(FromStd(item.hint));
        }
        status_ = lines.join('\n');
        emit changed();
        return;
    }

    if (!launch_.GameStarter().Start(command, error)) {
        status_ = QStringLiteral("启动失败：%1").arg(FromStd(error));
        emit changed();
        return;
    }

    busy_ = true;
    status_ = QStringLiteral("游戏已启动");
    emit changed();
}

void Bridge::repair() {
    if (busy_) return;
    busy_ = true;
    status_ = QStringLiteral("正在检查文件…");
    emit changed();

    std::thread([this] {
        download::installer::Transfer::Result report;
        std::string error;

        auto progress = [this](const std::string& label, int done, int total) {
            // 资源有几千个，别每个都往界面推
            if (done != total && done % 32 != 0) return;
            QMetaObject::invokeMethod(
                this,
                [this, label, done, total] {
                    status_ = QStringLiteral("%1/%2  %3").arg(done).arg(total).arg(FromStd(label));
                    emit changed();
                },
                Qt::QueuedConnection);
        };

        auto& installer = launch_.Downloader().Installers();
        const bool ok = installer.Versions().Install(installer.Transfers(), progress, report, error);
        QMetaObject::invokeMethod(
            this,
            [this, ok, report, error] {
                busy_ = false;
                status_ = ok ? QStringLiteral("文件已补齐：下载 %1，跳过 %2")
                                   .arg(report.downloaded)
                                   .arg(report.skipped)
                             : QStringLiteral("补齐失败：%1").arg(FromStd(error));
                UpdateDetails();
                emit changed();
            },
            Qt::QueuedConnection);
    }).detach();
}

ContentSource* Bridge::selectedSource() {
    auto sources = launch_.Downloader().Sources().Sources();
    if (content_source_ < 0 || static_cast<size_t>(content_source_) >= sources.size()) {
        return sources.empty() ? nullptr : sources.front();
    }
    return sources[static_cast<size_t>(content_source_)];
}

void Bridge::selectContentSource(int index) {
    content_source_ = index;
    emit downloadChanged();
}

void Bridge::loadGameVersions() {
    if (download_busy_) return;
    download_busy_ = true;
    download_status_ = QStringLiteral("正在获取版本清单…");
    emit downloadChanged();

    std::thread([this] {
        std::vector<download::versionsource::Mojang::Entry> entries;
        std::string error;
        const bool ok = launch_.Downloader().Sources().Game().List(entries, error);

        QStringList release, snapshot, april;
        QStringList release_ids, snapshot_ids, april_ids;
        if (ok) {
            for (const auto& entry : entries) {
                const QString id = FromStd(entry.id);
                switch (entry.kind) {
                    case download::versionsource::Mojang::Kind::Release:
                        release << id;
                        release_ids << id;
                        break;
                    case download::versionsource::Mojang::Kind::AprilFools:
                        april << id;
                        april_ids << id;
                        break;
                    default:
                        snapshot << id;
                        snapshot_ids << id;
                        break;
                }
            }
        }
        QMetaObject::invokeMethod(
            this,
            [this, ok, release, snapshot, april, release_ids, snapshot_ids, april_ids, error] {
                release_versions_ = release;
                snapshot_versions_ = snapshot;
                april_versions_ = april;
                release_ids_ = release_ids;
                snapshot_ids_ = snapshot_ids;
                april_ids_ = april_ids;
                download_status_ =
                    ok ? QStringLiteral("正式 %1 · 测试 %2 · 愚人节 %3")
                             .arg(release.size())
                             .arg(snapshot.size())
                             .arg(april.size())
                       : QStringLiteral("获取失败：%1").arg(FromStd(error));
                download_busy_ = false;
                emit downloadChanged();
            },
            Qt::QueuedConnection);
    }).detach();
}

void Bridge::installGame(int kind, int index, const QString& loader) {
    if (download_busy_) return;

    const QStringList* ids = &release_ids_;
    if (kind == 1) ids = &snapshot_ids_;
    if (kind == 2) ids = &april_ids_;
    if (index < 0 || index >= ids->size()) return;

    const std::string id = ids->at(index).toStdString();
    const std::string loader_name = loader.toStdString();
    const std::string game_dir = launch_.Versions().GameDir();

    if (loader_name != "原版" && loader_name != "Fabric") {
        download_status_ = QStringLiteral("%1 安装尚未接入").arg(loader);
        emit downloadChanged();
        return;
    }

    download_busy_ = true;
    download_progress_ = 0.0;
    download_status_ = QStringLiteral("正在安装 %1（%2）…").arg(FromStd(id)).arg(loader);
    emit downloadChanged();

    std::thread([this, id, loader_name, game_dir] {
        auto& sources = launch_.Downloader().Sources();
        std::string error;
        std::string version_json;
        std::string target_id = id;

        if (loader_name == "Fabric") {
            std::string loader_version;
            if (!sources.Loaders().FabricLatest(id, loader_version, error) ||
                !sources.Loaders().FabricProfile(id, loader_version, version_json, error)) {
                QMetaObject::invokeMethod(
                    this,
                    [this, error] {
                        download_status_ = QStringLiteral("Fabric 加载器获取失败：%1").arg(FromStd(error));
                        download_busy_ = false;
                        emit downloadChanged();
                    },
                    Qt::QueuedConnection);
                return;
            }

            Json::Value root;
            Json json;
            std::string parse_error;
            if (json.Parse(version_json, root, parse_error) && !root["id"].AsString().empty()) {
                target_id = root["id"].AsString();
            } else {
                target_id = "fabric-loader-" + loader_version + "-" + id;
            }
        } else {
            std::vector<download::versionsource::Mojang::Entry> entries;
            if (!sources.Game().List(entries, error)) {
                QMetaObject::invokeMethod(
                    this,
                    [this, error] {
                        download_status_ = QStringLiteral("获取清单失败：%1").arg(FromStd(error));
                        download_busy_ = false;
                        emit downloadChanged();
                    },
                    Qt::QueuedConnection);
                return;
            }

            std::string url;
            for (const auto& entry : entries) {
                if (entry.id == id) {
                    url = entry.url;
                    break;
                }
            }
            if (url.empty() || !sources.Game().Fetch(url, version_json, error)) {
                QMetaObject::invokeMethod(
                    this,
                    [this, id, error] {
                        download_status_ = QStringLiteral("拿不到 %1 的版本描述：%2")
                                               .arg(FromStd(id))
                                               .arg(FromStd(error));
                        download_busy_ = false;
                        emit downloadChanged();
                    },
                    Qt::QueuedConnection);
                return;
            }
        }

        download::installer::Version::WriteMarker(game_dir, target_id, loader_name, false);

        if (!download::installer::Version::WriteVersionJson(game_dir, target_id, version_json, error)) {
            QMetaObject::invokeMethod(
                this,
                [this, error] {
                    download_status_ = QStringLiteral("写入版本失败：%1").arg(FromStd(error));
                    download_busy_ = false;
                    emit downloadChanged();
                },
                Qt::QueuedConnection);
            return;
        }

        auto& installer = launch_.Downloader().Installers();
        download::installer::Transfer::Result result;
        auto progress = [this, target_id](const std::string& label, int done, int total) {
            if (done != total && done % 16 != 0) return;
            const double fraction = total > 0 ? static_cast<double>(done) / total : 0.0;
            QMetaObject::invokeMethod(
                this,
                [this, target_id, label, done, total, fraction] {
                    download_progress_ = fraction;
                    download_status_ = QStringLiteral("%1  %2/%3  %4")
                                           .arg(FromStd(target_id))
                                           .arg(done)
                                           .arg(total)
                                           .arg(FromStd(label));
                    emit downloadChanged();
                },
                Qt::QueuedConnection);
        };

        launch_.Versions().Refresh();
        const bool selected = launch_.Versions().Select(target_id);
        const bool ok =
            selected && installer.Versions().Install(installer.Transfers(), progress, result, error, true);

        QMetaObject::invokeMethod(
            this,
            [this, ok, target_id, result, error, game_dir, loader_name] {
                RefreshVersionList();
                launch_.Versions().Select(target_id);
                UpdateDetails();
                if (ok) download::installer::Version::WriteMarker(game_dir, target_id, loader_name, true);
                download_status_ = ok ? QStringLiteral("%1 安装完成：下载 %2，跳过 %3")
                                           .arg(FromStd(target_id))
                                           .arg(result.downloaded)
                                           .arg(result.skipped)
                                     : QStringLiteral("安装失败：%1").arg(FromStd(error));
                download_busy_ = false;
                emit downloadChanged();
                emit changed();
            },
            Qt::QueuedConnection);
    }).detach();
}

void Bridge::openVersionFolder(const QString& id) {
    const std::string dir = launch_.Versions().VersionDir(id.toStdString());
#ifdef _WIN32
    const std::wstring wide(dir.begin(), dir.end());
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    (void)dir;
#endif
}

void Bridge::deleteVersion(const QString& id) {
    const std::string dir = launch_.Versions().VersionDir(id.toStdString());
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::u8path(dir), ec);

    launch_.Versions().Refresh();
    RefreshVersionList();
    UpdateDetails();
    emit changed();
}
void Bridge::RefreshVersionList() {
    versions_.clear();
    for (const auto& entry : launch_.Versions().Entries()) {
        versions_ << FromStd(entry.id);
    }
    const std::string selected = launch_.Versions().Selected();
    if (!selected.empty()) selected_ = FromStd(selected);
}

void Bridge::searchContent(const QString& query, const QString& kind) {
    if (download_busy_) return;
    content_kind_ = kind;
    download_busy_ = true;
    download_status_ = QStringLiteral("正在搜索…");
    emit downloadChanged();

    const std::string text = query.toStdString();
    std::thread([this, text] {
        std::vector<ContentSource::Project> projects;
        std::string error;
        const bool ok =
            selectedSource()->Search(text, "", "", projects, error);

        QStringList titles;
        QStringList ids;
        if (ok) {
            for (const auto& project : projects) {
                titles << QStringLiteral("%1  ·  %2").arg(FromStd(project.title)).arg(FromStd(project.slug));
                ids << FromStd(project.id);
            }
        }
        QMetaObject::invokeMethod(
            this,
            [this, ok, titles, ids, error] {
                content_results_ = titles;
                content_ids_ = ids;
                download_status_ = ok ? QStringLiteral("找到 %1 个结果").arg(titles.size())
                                      : QStringLiteral("搜索失败：%1").arg(FromStd(error));
                download_busy_ = false;
                emit downloadChanged();
            },
            Qt::QueuedConnection);
    }).detach();
}

void Bridge::installContent(int index) {
    if (download_busy_ || index < 0 || index >= content_ids_.size()) return;

    const std::string project_id = content_ids_.at(index).toStdString();
    const std::string kind = content_kind_.toStdString();
    const std::string instance_dir = launch_.Versions().GameDir();

    download_busy_ = true;
    download_progress_ = 0.0;
    download_status_ = QStringLiteral("正在解析版本…");
    emit downloadChanged();

    std::thread([this, project_id, kind, instance_dir] {
        std::vector<ContentSource::Version> versions;
        std::string error;
        auto& installer = launch_.Downloader().Installers();

        if (!selectedSource()->Versions(project_id, "", "", versions, error)) {
            QMetaObject::invokeMethod(
                this,
                [this, error] {
                    download_status_ = QStringLiteral("解析失败：%1").arg(FromStd(error));
                    download_busy_ = false;
                    emit downloadChanged();
                },
                Qt::QueuedConnection);
            return;
        }
        if (versions.empty() || versions.front().files.empty()) {
            QMetaObject::invokeMethod(
                this,
                [this] {
                    download_status_ = QStringLiteral("这个项目没有可下载文件");
                    download_busy_ = false;
                    emit downloadChanged();
                },
                Qt::QueuedConnection);
            return;
        }

        const auto& file = versions.front().files.front();
        download::installer::Placement::Kind placement_kind = download::installer::Placement::Kind::Mod;
        if (kind == "shader") placement_kind = download::installer::Placement::Kind::Shaderpack;
        if (kind == "resourcepack") placement_kind = download::installer::Placement::Kind::Resourcepack;

        download::installer::Transfer::Task task;
        task.url = file.url;
        task.sha1 = file.sha1;
        task.dest = download::installer::Placement::Resolve(instance_dir, placement_kind, file.filename, false);
        task.label = file.filename;
        task.size = file.size;

        download::installer::Transfer::Result result;
        const bool ok = installer.Transfers().Run({task}, {}, result, error);

        QMetaObject::invokeMethod(
            this,
            [this, ok, error, file] {
                download_status_ = ok ? QStringLiteral("已安装 %1").arg(FromStd(file.filename))
                                      : QStringLiteral("安装失败：%1").arg(FromStd(error));
                download_busy_ = false;
                emit downloadChanged();
            },
            Qt::QueuedConnection);
    }).detach();
}

void Bridge::RefreshStatus() {
    if (!busy_) return;
    if (launch_.GameStarter().IsRunning()) return;

    busy_ = false;
    const int code = launch_.GameStarter().ExitCode();
    status_ = QStringLiteral("游戏已退出（退出码 %1）").arg(code);
    emit changed();
}
