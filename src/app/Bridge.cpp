#include "app/Bridge.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QMetaObject>
#include <QUrl>
#include <QVariantMap>

#include <cctype>
#include <filesystem>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#include "app/directory/Directory.h"
#include "app/download/Flow/Dispatcher.h"
#include "app/server/Installer/Installer.h"
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
    // 1.16~1.20 的 Forge/NeoForge 都从 modlauncher 启动；没有 rml.json 的外部版本靠这个兜底
    if (main_class.find("modlauncher") != std::string::npos) return QStringLiteral("Forge");
    return QStringLiteral("原版");
}

std::string ContentLoaderId(const std::string& loader, const std::string& id) {
    std::string value = loader;
    if (value.empty()) {
        if (id.find("-neoforge-") != std::string::npos) return "neoforge";
        if (id.find("-forge-") != std::string::npos) return "forge";
        if (id.find("-fabric-") != std::string::npos) return "fabric";
        if (id.find("-quilt-") != std::string::npos) return "quilt";
        return {};
    }
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (value == "forge") return "forge";
    if (value == "neoforge") return "neoforge";
    if (value == "fabric") return "fabric";
    if (value == "quilt") return "quilt";
    return {};
}

bool ContainsValue(const std::vector<std::string>& values, const std::string& wanted) {
    for (const auto& value : values) {
        if (value == wanted) return true;
    }
    return false;
}

QString SourceKindLabel(const std::string& kind) {
    if (kind == "mojang") return QStringLiteral("Mojang 版本清单");
    if (kind == "assets") return QStringLiteral("Mojang 资源");
    if (kind == "forge") return QStringLiteral("Forge maven");
    if (kind == "neoforge") return QStringLiteral("NeoForge maven");
    if (kind == "fabric") return QStringLiteral("Fabric meta");
    return FromStd(kind);
}

// 背景图带时间戳，换图时 URL 会变，QML 的 Image 才会重新加载
QString CustomBackgroundFile() {
    QDir dir(FromStd(Directory::DataRoot()));
    const QStringList files =
        dir.entryList({QStringLiteral("background*.png")}, QDir::Files, QDir::Time);
    for (const QString& name : files) {
        const QString path = dir.filePath(name);
        if (QFile::exists(path)) return path;
    }
    return {};
}

void RemoveCustomBackgrounds(const QString& keep = QString()) {
    QDir dir(FromStd(Directory::DataRoot()));
    for (const QString& name : dir.entryList({QStringLiteral("background*.png")}, QDir::Files)) {
        const QString path = dir.filePath(name);
        if (!keep.isEmpty() && path == keep) continue;
        QFile::remove(path);
    }
}

}  // namespace

Bridge::Bridge(Launch& launch, QString account_name, int memory_mb, QObject* parent)
    : QObject(parent),
      launch_(launch),
      account_name_(std::move(account_name)),
      memory_mb_(memory_mb) {
    RefreshVersionList();

    if (!versions_.isEmpty()) {
        // 尊重 settings.json 里的选择；只有没有选择时才退到第一个
        const std::string saved = launch_.Versions().Selected();
        if (!saved.empty() && version_ids_.contains(FromStd(saved))) {
            selected_ = FromStd(saved);
            selected_index_ = version_ids_.indexOf(selected_);
        } else {
            selected_index_ = 0;
            selected_ = versions_.first();
            launch_.Versions().Select(version_ids_.first().toStdString());
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
    const QString custom = CustomBackgroundFile();
    if (!custom.isEmpty()) return QUrl::fromLocalFile(custom).toString();

    // 部署后优先用 exe 旁边的 qml/assets
    const QString local = QCoreApplication::applicationDirPath() +
                          QStringLiteral("/qml/assets/background.png");
    if (QFile::exists(local)) return QUrl::fromLocalFile(local).toString();

    return QUrl::fromLocalFile(QStringLiteral(RML_QML_DIR "/assets/background.png")).toString();
}

bool Bridge::HasCustomBackground() const { return !CustomBackgroundFile().isEmpty(); }

void Bridge::setBackground(const QString& path) {
    // QML 的 FileDialog 给的是 file:/// URL，这里统一成本地路径
    QString local = path;
    const QUrl url(path);
    if (url.isLocalFile()) local = url.toLocalFile();

    const QImage image(local);
    if (image.isNull()) {
        status_ = QStringLiteral("背景图读取失败：%1").arg(local);
        emit changed();
        return;
    }

    const QString data_root = FromStd(Directory::DataRoot());
    QDir().mkpath(data_root);
    const QString target =
        data_root + QStringLiteral("/background-%1.png")
                       .arg(QDateTime::currentMSecsSinceEpoch());
    if (!image.save(target, "PNG")) {
        status_ = QStringLiteral("背景图保存失败：%1").arg(target);
        emit changed();
        return;
    }

    RemoveCustomBackgrounds(target);
    status_ = QStringLiteral("背景图已更新");
    emit changed();
}

void Bridge::resetBackground() {
    RemoveCustomBackgrounds();
    status_ = QStringLiteral("已恢复默认背景");
    emit changed();
}

QString Bridge::SelectedName() const {
    if (selected_index_ >= 0 && selected_index_ < versions_.size()) {
        return versions_.at(selected_index_);
    }
    return selected_;
}

void Bridge::applyLaunchOptions(int memoryMb, const QString& gc, const QString& jvmArgs) {
    // 小于 512 的值只在前端提示，不覆盖；否则用户逐位输入时会被改成 512
    if (memoryMb >= 512) memory_mb_ = memoryMb;
    gc_mode_ = gc;
    jvm_args_ = jvmArgs;

    Json::Value root = Json::Value::Object();
    root.Set("gameDir", Json::Value::String(launch_.Versions().GameDir()));
    root.Set("selectedVersion", Json::Value::String(launch_.Versions().Selected()));
    root.Set("username", Json::Value::String(account_name_.toStdString()));
    root.Set("memoryMb", Json::Value::Int(memory_mb_));
    root.Set("gc", Json::Value::String(gc_mode_.toStdString()));
    root.Set("jvmArgs", Json::Value::String(jvm_args_.toStdString()));

    const std::string settings_path = Directory::SettingsFile();
    tools::WriteTextFile(settings_path, root.Dump());
    emit changed();
}

QVariantList Bridge::SourceKindOptions() const {
    QVariantList out;
    for (const auto& kind : download::versionsource::Sources::Kinds()) {
        QVariantMap map;
        map["kind"] = FromStd(kind);
        map["label"] = SourceKindLabel(kind);
        out.append(map);
    }
    return out;
}

QVariantList Bridge::SourceRecords() const {
    QVariantList out;
    for (const auto& record : launch_.Downloader().Sources().Origins().Records()) {
        QVariantMap map;
        map["kind"] = FromStd(record.kind);
        map["kindLabel"] = SourceKindLabel(record.kind);
        map["name"] = FromStd(record.name);
        map["url"] = FromStd(record.url);
        out.append(map);
    }
    return out;
}

void Bridge::saveSource(int index, const QString& name, const QString& url) {
    auto& origins = launch_.Downloader().Sources().Origins();
    if (index < 0 || static_cast<size_t>(index) >= origins.Records().size()) return;
    origins.Update(static_cast<size_t>(index), name.trimmed().toStdString(),
                   url.trimmed().toStdString());
    SaveSources();
}

void Bridge::addSource(const QString& kind, const QString& name, const QString& url) {
    if (!download::versionsource::Sources::KnownKind(kind.toStdString())) {
        status_ = QStringLiteral("不认识的软件源类型：%1").arg(kind);
        emit changed();
        return;
    }
    launch_.Downloader().Sources().Origins().Add(kind.toStdString(), name.trimmed().toStdString(),
                                                 url.trimmed().toStdString());
    SaveSources();
}

void Bridge::removeSource(int index) {
    auto& origins = launch_.Downloader().Sources().Origins();
    if (index < 0 || static_cast<size_t>(index) >= origins.Records().size()) return;
    origins.Remove(static_cast<size_t>(index));
    SaveSources();
}

void Bridge::resetSources() {
    launch_.Downloader().Sources().Origins().ResetDefaults();
    SaveSources();
}

void Bridge::SaveSources() {
    std::string error;
    if (!launch_.Downloader().Sources().Origins().Save(error)) {
        status_ = QStringLiteral("软件源保存失败：%1").arg(FromStd(error));
    } else {
        status_ = QStringLiteral("软件源已保存");
    }
    emit sourcesChanged();
    emit changed();
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
        if (!entry.loader.empty()) loader_ = FromStd(entry.loader);
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

    repair_failed_ = false;
    repairing_ = false;
    cancelling_ = false;
    cancel_requested_ = false;

    AccountProvider::Credentials credentials;
    credentials.username = account_name_.toStdString();
    credentials.uuid = "00000000-0000-0000-0000-000000000000";
    credentials.access_token = "0";
    credentials.user_type = "legacy";

    launch::Starter::Options options;
    options.memory_mb = memory_mb_;
    options.java_path = java_path_override_.toStdString();
    options.gc = gc_mode_.toStdString();
    options.jvm_args = jvm_args_.toStdString();
    launch::Starter::Command command;
    std::vector<launch::Starter::Missing> missing;
    std::string error;

    // 缺文件不再让用户点“补齐”，直接作为错误处理路径自动补
    if (!launch_.GameStarter().Build(credentials, options, command, missing, error)) {
        if (missing.empty()) {
            QStringList lines;
            lines << QStringLiteral("无法启动：%1").arg(FromStd(error));
            status_ = lines.join('\n');
            repair_failed_ = true;
            emit changed();
            return;
        }

        auto& transfers = launch_.Downloader().Installers().Transfers();
        transfers.ResetCancel();
        repair_progress_ = 0.0;
        repairing_ = true;
        cancelling_ = false;
        cancel_requested_ = false;
        busy_ = true;
        status_ = QStringLiteral("发现 %1 项文件缺失，正在自动补齐…").arg(missing.size());
        emit changed();

        std::thread([this, credentials, options, missing] {
            download::installer::Transfer::Result report;
            std::string repair_error;
            auto progress = [this](const std::string& label, int done, int total) {
                if (done != total && done % 32 != 0) return;
                const double fraction = total > 0 ? static_cast<double>(done) / total : 0.0;
                QMetaObject::invokeMethod(
                    this,
                    [this, label, done, total, fraction] {
                        repair_progress_ = fraction;
                        status_ = QStringLiteral("自动补齐 %1/%2  %3")
                                      .arg(done)
                                      .arg(total)
                                      .arg(FromStd(label));
                        emit changed();
                    },
                    Qt::QueuedConnection);
            };

            auto& installer = launch_.Downloader().Installers();
            const bool repaired = installer.Versions().Install(installer.Transfers(), progress,
                                                               report, repair_error);

            const bool was_cancelled = cancel_requested_.load();
            launch::Starter::Command retry_command;
            std::vector<launch::Starter::Missing> retry_missing;
            std::string retry_error;
            const bool built =
                repaired && !was_cancelled &&
                launch_.GameStarter().Build(credentials, options, retry_command, retry_missing,
                                            retry_error);

            bool started = false;
            if (built && !cancel_requested_.load()) {
                started = launch_.GameStarter().Start(retry_command, retry_error);
            }

            QMetaObject::invokeMethod(
                this,
                [this, repaired, built, started, report, repair_error, retry_error, retry_missing] {
                    busy_ = false;
                    repairing_ = false;
                    cancelling_ = false;
                    if (cancel_requested_.load()) {
                        repair_failed_ = false;
                        repair_progress_ = 0.0;
                        status_ = QStringLiteral("已取消自动补齐");
                    } else if (!repaired) {
                        repair_failed_ = true;
                        status_ = QStringLiteral("自动补齐失败：%1").arg(FromStd(repair_error));
                    } else if (!built) {
                        repair_failed_ = true;
                        QStringList lines;
                        lines << QStringLiteral("补齐后仍无法启动：%1").arg(FromStd(retry_error));
                        for (const auto& item : retry_missing) {
                            lines << QStringLiteral("· %1（%2）").arg(FromStd(item.what)).arg(FromStd(item.hint));
                        }
                        status_ = lines.join('\n');
                    } else if (!started) {
                        repair_failed_ = true;
                        status_ = QStringLiteral("启动失败：%1").arg(FromStd(retry_error));
                    } else {
                        busy_ = true;
                        repair_failed_ = false;
                        repair_progress_ = 1.0;
                        status_ = QStringLiteral("已自动补齐 %1 项，游戏已启动").arg(report.downloaded);
                    }
                    emit changed();
                },
                Qt::QueuedConnection);
        }).detach();
        return;
    }
    if (!launch_.GameStarter().Start(command, error)) {
        status_ = QStringLiteral("启动失败：%1").arg(FromStd(error));
        repair_failed_ = true;
        repairing_ = false;
        emit changed();
        return;
    }

    busy_ = true;
    repair_failed_ = false;
    repairing_ = false;
    status_ = QStringLiteral("游戏已启动");
    emit changed();
}

void Bridge::cancelLaunchRepair() {
    if (!repairing_ || cancelling_) return;
    cancelling_ = true;
    cancel_requested_ = true;
    launch_.Downloader().Installers().Transfers().Cancel();
    status_ = QStringLiteral("正在取消自动补齐…");
    emit changed();
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
    download_failed_ = false;
    download_progress_ = 0.0;
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
                download_failed_ = !ok;
                download_progress_ = ok ? 1.0 : 0.0;
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

    const std::string mc_version = ids->at(index).toStdString();
    const std::string loader_name = loader.toStdString();

    const bool known = loader_name == "原版" || loader_name == "Fabric" ||
                       loader_name == "Forge" || loader_name == "NeoForge";
    if (!known) {
        download_status_ = QStringLiteral("不认识的加载器：%1").arg(loader);
        emit downloadChanged();
        return;
    }

    download_busy_ = true;
    download_failed_ = false;
    download_progress_ = 0.0;
    installing_version_ = FromStd(mc_version);
    launch_.Downloader().Installers().Transfers().ResetCancel();
    download_status_ = QStringLiteral("正在安装 %1（%2）…").arg(FromStd(mc_version)).arg(loader);
    emit downloadChanged();

    std::thread([this, mc_version, loader_name] {
        download::flow::Context context{launch_.Versions(), launch_.Tools(),
                                        launch_.Downloader().Sources(),
                                        launch_.Downloader().Installers()};

        download::flow::Request request;
        request.game_dir = launch_.Versions().GameDir();
        request.mc_version = mc_version;
        request.loader = loader_name;

        auto progress = [this](const std::string& label, int done, int total) {
            if (done != total && done % 4 != 0) return;
            const double fraction = total > 0 ? static_cast<double>(done) / total : 0.0;
            QMetaObject::invokeMethod(
                this,
                [this, label, fraction] {
                    download_progress_ = fraction;
                    download_status_ = QStringLiteral("%1  %2%")
                                           .arg(FromStd(label))
                                           .arg(static_cast<int>(fraction * 100.0));
                    emit downloadChanged();
                },
                Qt::QueuedConnection);
        };

        download::flow::Outcome outcome;
        std::string error;
        const bool ok = download::flow::Dispatcher().Run(context, request, progress, outcome, error);

        QMetaObject::invokeMethod(
            this,
            [this, ok, outcome, error, loader_name] {
                installing_version_.clear();
                download_busy_ = false;
                download_failed_ = !ok;
                RefreshVersionList();
                if (ok) download_progress_ = 1.0;
                if (ok) {
                    launch_.Versions().Select(outcome.version_id);
                    const int row = version_ids_.indexOf(FromStd(outcome.version_id));
                    if (row >= 0) {
                        selected_ = FromStd(outcome.version_id);
                        selected_index_ = row;
                    }
                    UpdateDetails();
                    const QString version_tag = outcome.loader_version.empty()
                                                    ? QString()
                                                    : QStringLiteral(" %1").arg(FromStd(outcome.loader_version));
                    download_status_ = QStringLiteral("安装完成：%1（%2%3）")
                                           .arg(FromStd(outcome.version_id))
                                           .arg(FromStd(loader_name))
                                           .arg(version_tag);
                } else {
                    download_status_ = QStringLiteral("安装失败：%1").arg(FromStd(error));
                }
                emit downloadChanged();
                emit changed();
            },
            Qt::QueuedConnection);
    }).detach();
}

void Bridge::installServer(int kind, int index, const QString& loader) {
    if (download_busy_) return;

    const QStringList* ids = &release_ids_;
    if (kind == 1) ids = &snapshot_ids_;
    if (kind == 2) ids = &april_ids_;
    if (index < 0 || index >= ids->size()) return;

    const std::string mc_version = ids->at(index).toStdString();
    const std::string loader_name = loader.toStdString();
    if (loader_name != "原版" && loader_name != "Fabric") {
        download_status_ = QStringLiteral("服务端暂不支持 %1（当前只有原版和 Fabric）").arg(loader);
        emit downloadChanged();
        return;
    }

    download_busy_ = true;
    download_failed_ = false;
    download_progress_ = 0.0;
    launch_.Downloader().Installers().Transfers().ResetCancel();
    download_status_ = QStringLiteral("正在下载 %1 服务端（%2）…")
                           .arg(FromStd(mc_version))
                           .arg(loader);
    emit downloadChanged();

    std::thread([this, mc_version, loader_name] {
        server::Installer installer(launch_.Versions(), launch_.Tools(),
                                    launch_.Downloader().Sources(),
                                    launch_.Downloader().Installers().Transfers());

        auto progress = [this](const std::string& label, int done, int total) {
            if (done != total && done % 16 != 0) return;
            const double fraction = total > 0 ? static_cast<double>(done) / total : 0.0;
            QMetaObject::invokeMethod(
                this,
                [this, label, fraction] {
                    download_progress_ = fraction;
                    download_status_ = QStringLiteral("%1  %2%")
                                           .arg(FromStd(label))
                                           .arg(static_cast<int>(fraction * 100.0));
                    emit downloadChanged();
                },
                Qt::QueuedConnection);
        };

        std::string version_id;
        std::string error;
        const bool ok = loader_name == "Fabric"
                            ? installer.Fabric(mc_version, progress, version_id, error)
                            : installer.Vanilla(mc_version, progress, version_id, error);

        QMetaObject::invokeMethod(
            this,
            [this, ok, version_id, error] {
                download_busy_ = false;
                download_failed_ = !ok;
                if (ok) download_progress_ = 1.0;
                download_status_ = ok ? QStringLiteral("服务端已下载：%1").arg(FromStd(version_id))
                                      : QStringLiteral("服务端下载失败：%1").arg(FromStd(error));
                emit downloadChanged();
            },
            Qt::QueuedConnection);
    }).detach();
}


std::string Bridge::versionIdAt(int index) const {
    if (index < 0 || index >= version_ids_.size()) return {};
    return version_ids_.at(index).toStdString();
}

const Directory::VersionEntry* Bridge::SelectedEntry() const {
    for (const auto& entry : launch_.Versions().Entries()) {
        if (entry.id == selected_.toStdString()) return &entry;
    }
    return nullptr;
}

std::string Bridge::SelectedMinecraftVersion() const {
    const Directory::VersionEntry* entry = SelectedEntry();
    return entry == nullptr ? std::string() : entry->minecraft_version;
}

std::string Bridge::SelectedContentLoader() const {
    const Directory::VersionEntry* entry = SelectedEntry();
    if (entry == nullptr) return {};
    return ContentLoaderId(entry->loader, entry->id);
}

void Bridge::selectVersionAt(int index) {
    const std::string id = versionIdAt(index);
    if (id.empty() || !launch_.Versions().Select(id)) return;
    selected_ = FromStd(id);
    selected_index_ = index;
    UpdateDetails();
    emit changed();
}

void Bridge::openVersionFolderAt(int index) {
    const std::string id = versionIdAt(index);
    if (id.empty()) return;
    const std::string dir = launch_.Versions().VersionDir(id);
#ifdef _WIN32
    const std::wstring wide(dir.begin(), dir.end());
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    (void)dir;
#endif
}

void Bridge::deleteVersionAt(int index) {
    const std::string id = versionIdAt(index);
    if (id.empty()) return;

    const std::string dir = launch_.Versions().VersionDir(id);
    std::error_code ec;
    std::filesystem::remove_all(tools::ToPath(dir), ec);

    launch_.Versions().Refresh();
    RefreshVersionList();
    if (versions_.isEmpty()) {
        selected_.clear();
        selected_index_ = -1;
        details_.clear();
    } else {
        selectVersionAt(qMin(index, static_cast<int>(versions_.size()) - 1));
    }
    emit changed();
}

void Bridge::renameVersionAt(int index, const QString& name) {
    const std::string id = versionIdAt(index);
    if (id.empty() || name.trimmed().isEmpty()) return;
    launch_.Versions().Rename(id, name.trimmed().toStdString());
    RefreshVersionList();
    emit changed();
}


void Bridge::RefreshVersionList() {
    launch_.Versions().Refresh();
    versions_.clear();
    version_ids_.clear();
    for (const auto& entry : launch_.Versions().Entries()) {
        // 服务端安装不参与启动页
        if (entry.is_server) continue;
        // 安装中途失败的托管版本不进入启动页，避免点进去直接退出码 1
        if (entry.managed && !entry.complete) continue;
        versions_ << FromStd(entry.name);
        version_ids_ << FromStd(entry.id);
    }

    selected_ = FromStd(launch_.Versions().Selected());
    selected_index_ = version_ids_.indexOf(selected_);
}

void Bridge::searchContent(const QString& query, const QString& kind) {
    if (download_busy_) return;

    const std::string mc_version = SelectedMinecraftVersion();
    const std::string loader = SelectedContentLoader();
    if (mc_version.empty()) {
        download_status_ = QStringLiteral("先选择一个游戏版本，再搜索内容");
        emit downloadChanged();
        return;
    }
    if (kind == QStringLiteral("mod") && loader.empty()) {
        download_status_ = QStringLiteral("当前版本没有 Mod 加载器，不能安装 Mod");
        emit downloadChanged();
        return;
    }

    content_kind_ = kind;
    download_busy_ = true;
    download_failed_ = false;
    download_status_ = QStringLiteral("正在搜索…");
    emit downloadChanged();

    const std::string text = query.toStdString();
    std::thread([this, text, loader, mc_version] {
        std::vector<ContentSource::Project> projects;
        std::string error;
        bool ok = selectedSource()->Search(text, loader, mc_version, projects, error);

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
                download_failed_ = !ok;
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
    const std::string mc_version = SelectedMinecraftVersion();
    const std::string loader = SelectedContentLoader();
    if (mc_version.empty()) {
        download_status_ = QStringLiteral("先选择一个游戏版本，再安装内容");
        emit downloadChanged();
        return;
    }
    if (kind == "mod" && loader.empty()) {
        download_status_ = QStringLiteral("当前版本没有 Mod 加载器，不能安装 Mod");
        emit downloadChanged();
        return;
    }

    download_busy_ = true;
    download_failed_ = false;
    download_progress_ = 0.0;
    launch_.Downloader().Installers().Transfers().ResetCancel();
    download_status_ = QStringLiteral("正在查找适配 %1 的版本…").arg(FromStd(mc_version));
    emit downloadChanged();

    std::thread([this, project_id, kind, instance_dir, loader, mc_version] {
        std::vector<ContentSource::Version> versions;
        std::string error;
        auto& installer = launch_.Downloader().Installers();

        if (!selectedSource()->Versions(project_id, loader, mc_version, versions, error)) {
            QMetaObject::invokeMethod(
                this,
                [this, error] {
                    download_status_ = QStringLiteral("解析失败：%1").arg(FromStd(error));
                    download_busy_ = false;
                    download_failed_ = true;
                    emit downloadChanged();
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
                this,
                [this, reason] {
                    download_status_ = reason;
                    download_busy_ = false;
                    download_failed_ = true;
                    emit downloadChanged();
                },
                Qt::QueuedConnection);
            return;
        }

        const auto& file = chosen->files.front();
        download::installer::Placement::Kind placement_kind = download::installer::Placement::Kind::Mod;
        if (kind == "shader") placement_kind = download::installer::Placement::Kind::Shaderpack;
        if (kind == "resourcepack") placement_kind = download::installer::Placement::Kind::Resourcepack;

        download::installer::Transfer::Task task;
        task.url = file.url;
        task.sha1 = file.sha1;
        task.dest = download::installer::Placement::Resolve(instance_dir, placement_kind,
                                                            file.filename, true);
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
                download_failed_ = !ok;
                if (ok) download_progress_ = 1.0;
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
