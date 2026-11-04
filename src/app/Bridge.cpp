#include "app/Bridge.h"

#include <QMetaObject>

#include <string>
#include <thread>
#include <vector>

#include "app/directory/Directory.h"
#include "app/download/Installer/Transfer/Transfer.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"
#include "include/AccountProvider.h"

Bridge::Bridge(Launch& launch, QString account_name, int memory_mb, QObject* parent)
    : QObject(parent),
      launch_(launch),
      account_name_(std::move(account_name)),
      memory_mb_(memory_mb),
      versions_(launch),
      downloads_(
          launch, this, [this] { emit downloadChanged(); }, [this] { emit changed(); },
          [this](const std::string& id) { versions_.SelectInstalled(id); }) {
    connect(&status_timer_, &QTimer::timeout, this, &Bridge::RefreshStatus);
    status_timer_.start(1000);
}

QString Bridge::GameDir() const { return QString::fromStdString(launch_.Versions().GameDir()); }

QString Bridge::BackgroundUrl() const { return BridgeBackground::Url(); }

bool Bridge::HasCustomBackground() const { return BridgeBackground::Has(); }

QVariantList Bridge::SourceRecords() const {
    return BridgeSourceRecords::Records(launch_.Downloader().Sources().Origins());
}

QVariantList Bridge::SourceKindOptions() const { return BridgeSourceRecords::KindOptions(); }

void Bridge::setBackground(const QString& path) {
    QString error;
    if (!BridgeBackground::Set(path, error)) {
        status_ = error;
        emit changed();
        return;
    }

    status_ = QStringLiteral("背景图已更新");
    emit changed();
}

void Bridge::resetBackground() {
    BridgeBackground::Reset();
    status_ = QStringLiteral("已恢复默认背景");
    emit changed();
}

void Bridge::saveSource(int index, const QString& name, const QString& url) {
    auto& origins = launch_.Downloader().Sources().Origins();
    if (BridgeSourceRecords::Update(origins, index, name, url)) SaveSources();
}

void Bridge::addSource(const QString& kind, const QString& name, const QString& url) {
    QString error;
    if (!BridgeSourceRecords::Add(launch_.Downloader().Sources().Origins(), kind, name, url,
                                  error)) {
        status_ = error;
        emit changed();
        return;
    }
    SaveSources();
}

void Bridge::removeSource(int index) {
    if (BridgeSourceRecords::Remove(launch_.Downloader().Sources().Origins(), index)) {
        SaveSources();
    }
}

void Bridge::resetSources() {
    BridgeSourceRecords::Reset(launch_.Downloader().Sources().Origins());
    SaveSources();
}

void Bridge::SaveSources() {
    QString error;
    if (!BridgeSourceRecords::Save(launch_.Downloader().Sources().Origins(), error)) {
        status_ = QStringLiteral("软件源保存失败：%1").arg(error);
    } else {
        status_ = QStringLiteral("软件源已保存");
    }
    emit sourcesChanged();
    emit changed();
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

void Bridge::selectVersion(const QString& id) {
    if (versions_.Select(id)) emit changed();
}

void Bridge::selectVersionAt(int index) {
    if (versions_.SelectAt(index)) emit changed();
}

void Bridge::openVersionFolderAt(int index) { versions_.OpenFolderAt(index); }

void Bridge::deleteVersionAt(int index) {
    versions_.DeleteAt(index);
    emit changed();
}

void Bridge::renameVersionAt(int index, const QString& name) {
    versions_.RenameAt(index, name);
    emit changed();
}

void Bridge::selectJava(int index) {
    versions_.SelectJava(index);
    emit changed();
}

void Bridge::installJava(int major) {
    if (busy_) return;
    busy_ = true;
    game_started_ = false;
    status_ = QStringLiteral("正在下载并安装 Java %1…").arg(major);
    emit changed();

    std::thread([this, major] {
        std::string error;
        const bool ok = launch_.Tools().Install(major, error);
        QMetaObject::invokeMethod(
            this,
            [this, ok, error, major] {
                if (ok) {
                    versions_.RefreshJavas();
                    versions_.UpdateDetails();
                    status_ = QStringLiteral("Java %1 已安装到 RML 的运行时目录").arg(major);
                } else {
                    status_ = QStringLiteral("安装失败：%1").arg(QString::fromStdString(error));
                }
                busy_ = false;
                emit changed();
            },
            Qt::QueuedConnection);
    }).detach();
}

void Bridge::launchGame() {
    if (busy_) return;

    Directory paths(launch_.Versions().GameDir());
    std::string mods_error;
    const std::string content_loader = versions_.SelectedContentLoader();
    const Directory::VersionEntry* entry = versions_.SelectedEntry();
    if (!content_loader.empty() && entry != nullptr) {
        // 有加载器的版本使用独立 gameDir，Forge/Fabric 才会加载 versions/<id>/mods
        if (!paths.EnsureInstalledModsRoot(entry->id, mods_error)) {
            status_ = QString::fromStdString(mods_error);
            emit changed();
            return;
        }
    } else if (!paths.EnsureModsRoot(mods_error)) {
        status_ = QString::fromStdString(mods_error);
        emit changed();
        return;
    }

    repair_failed_ = false;
    repairing_ = false;
    cancelling_ = false;
    cancel_requested_ = false;
    game_started_ = false;

    AccountProvider::Credentials credentials;
    credentials.username = account_name_.toStdString();
    credentials.uuid = "00000000-0000-0000-0000-000000000000";
    credentials.access_token = "0";
    credentials.user_type = "legacy";

    launch::Starter::Options options;
    options.memory_mb = memory_mb_;
    options.java_path = versions_.JavaPathOverride().toStdString();
    options.gc = gc_mode_.toStdString();
    options.jvm_args = jvm_args_.toStdString();
    if (!content_loader.empty() && entry != nullptr) {
        options.game_dir = paths.InstalledGameRoot(entry->id);
    }
    launch::Starter::Command command;
    std::vector<launch::Starter::Missing> missing;
    std::string error;

    // 缺文件不再让用户点“补齐”，直接作为错误处理路径自动补
    if (!launch_.GameStarter().Build(credentials, options, command, missing, error)) {
        if (missing.empty()) {
            status_ = QStringLiteral("无法启动：%1").arg(QString::fromStdString(error));
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
                                      .arg(QString::fromStdString(label));
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
                    game_started_ = started;
                    repairing_ = false;
                    cancelling_ = false;
                    if (cancel_requested_.load()) {
                        repair_failed_ = false;
                        repair_progress_ = 0.0;
                        status_ = QStringLiteral("已取消自动补齐");
                    } else if (!repaired) {
                        repair_failed_ = true;
                        status_ = QStringLiteral("自动补齐失败：%1")
                                      .arg(QString::fromStdString(repair_error));
                    } else if (!built) {
                        repair_failed_ = true;
                        QStringList lines;
                        lines << QStringLiteral("补齐后仍无法启动：%1")
                                     .arg(QString::fromStdString(retry_error));
                        for (const auto& item : retry_missing) {
                            lines << QStringLiteral("· %1（%2）")
                                         .arg(QString::fromStdString(item.what))
                                         .arg(QString::fromStdString(item.hint));
                        }
                        status_ = lines.join('\n');
                    } else if (!started) {
                        repair_failed_ = true;
                        status_ =
                            QStringLiteral("启动失败：%1").arg(QString::fromStdString(retry_error));
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
        status_ = QStringLiteral("启动失败：%1").arg(QString::fromStdString(error));
        repair_failed_ = true;
        repairing_ = false;
        game_started_ = false;
        emit changed();
        return;
    }

    busy_ = true;
    game_started_ = true;
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

void Bridge::loadGameVersions() { downloads_.LoadGameVersions(); }

void Bridge::installGame(int kind, int index, const QString& loader) {
    downloads_.InstallGame(kind, index, loader);
}

void Bridge::installGameWithLoader(int kind, int index, const QString& loader,
                                   const QString& loaderVersion) {
    downloads_.InstallGame(kind, index, loader, loaderVersion);
}

void Bridge::loadLoaderVersions(const QString& mcVersion, const QString& loader) {
    downloads_.LoadLoaderVersions(mcVersion, loader);
}

void Bridge::installServer(int kind, int index, const QString& loader) {
    downloads_.InstallServer(kind, index, loader);
}

void Bridge::searchContent(const QString& query, const QString& kind) {
    downloads_.SearchContent(query, kind, versions_.SelectedMinecraftVersion(),
                             versions_.SelectedContentLoader());
}

void Bridge::installContent(int index) {
    downloads_.InstallContent(index, versions_.SelectedMinecraftVersion(),
                              versions_.SelectedContentLoader(), launch_.Versions().GameDir());
}

void Bridge::selectContentSource(int index) { downloads_.SelectSource(index); }

QVariantList Bridge::installedMods() {
    const Directory::VersionEntry* entry = versions_.SelectedEntry();
    if (entry == nullptr) return {};
    return mods_.Installed(launch_.Versions().GameDir(), *entry);
}

QVariantList Bridge::repositoryMods() {
    const Directory::VersionEntry* entry = versions_.SelectedEntry();
    if (entry == nullptr) return {};
    return mods_.Repository(launch_.Versions().GameDir(), *entry);
}

bool Bridge::installRepositoryMod(int index) {
    const Directory::VersionEntry* entry = versions_.SelectedEntry();
    if (entry == nullptr) return false;

    QString message;
    const bool ok = mods_.Install(launch_.Versions().GameDir(), *entry, index, message);
    if (message.isEmpty()) return false;
    downloads_.SetStatus(message);
    return ok;
}

bool Bridge::removeInstalledMod(int index) {
    const Directory::VersionEntry* entry = versions_.SelectedEntry();
    if (entry == nullptr) return false;

    QString message;
    const bool ok = mods_.Remove(launch_.Versions().GameDir(), *entry, index, message);
    if (message.isEmpty()) return false;
    downloads_.SetStatus(message);
    return ok;
}

void Bridge::RefreshStatus() {
    // 只有本启动器确实拉起过游戏进程时才轮询退出码。
    // Java 安装、版本下载等 busy_ 不等于“游戏已启动”，否则会误报退出码 -1。
    if (!game_started_) return;
    if (launch_.GameStarter().IsRunning()) return;

    game_started_ = false;
    busy_ = false;
    const int code = launch_.GameStarter().ExitCode();
    status_ = QStringLiteral("游戏已退出（退出码 %1）").arg(code);
    emit changed();
}
