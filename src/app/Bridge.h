// 应用层：把启动模块暴露给 QML。不属于任何模块。
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include "app/bridge/Background.h"
#include "app/bridge/Downloads.h"
#include "app/bridge/Mods.h"
#include "app/bridge/SourceRecords.h"
#include "app/bridge/Tasks.h"
#include "app/bridge/Versions.h"
#include "app/launch/Launch.h"

class Bridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList versions READ Versions NOTIFY changed)
    Q_PROPERTY(QString selectedName READ SelectedName NOTIFY changed)
    Q_PROPERTY(QString details READ Details NOTIFY changed)
    Q_PROPERTY(QString javaInfo READ JavaInfo NOTIFY changed)
    Q_PROPERTY(QString status READ Status NOTIFY changed)
    Q_PROPERTY(bool busy READ Busy NOTIFY changed)
    Q_PROPERTY(QStringList javas READ Javas NOTIFY changed)
    Q_PROPERTY(QString loader READ Loader NOTIFY changed)
    Q_PROPERTY(QString backgroundUrl READ BackgroundUrl NOTIFY changed)
    Q_PROPERTY(bool hasCustomBackground READ HasCustomBackground NOTIFY changed)
    Q_PROPERTY(QVariantList sourceRecords READ SourceRecords NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList sourceKindOptions READ SourceKindOptions CONSTANT)
    Q_PROPERTY(QStringList contentResults READ ContentResults NOTIFY downloadChanged)
    Q_PROPERTY(QStringList contentNames READ ContentNames NOTIFY downloadChanged)
    Q_PROPERTY(QStringList contentIcons READ ContentIcons NOTIFY downloadChanged)
    Q_PROPERTY(QStringList contentDescriptions READ ContentDescriptions NOTIFY downloadChanged)
    Q_PROPERTY(QString downloadStatus READ DownloadStatus NOTIFY downloadChanged)
    Q_PROPERTY(bool downloadBusy READ DownloadBusy NOTIFY downloadChanged)
    Q_PROPERTY(QStringList contentSources READ ContentSources NOTIFY downloadChanged)
    Q_PROPERTY(QStringList releaseVersions READ ReleaseVersions NOTIFY downloadChanged)
    Q_PROPERTY(QStringList snapshotVersions READ SnapshotVersions NOTIFY downloadChanged)
    Q_PROPERTY(QStringList aprilVersions READ AprilVersions NOTIFY downloadChanged)
    Q_PROPERTY(double downloadProgress READ DownloadProgress NOTIFY downloadChanged)
    Q_PROPERTY(bool downloadFailed READ DownloadFailed NOTIFY downloadChanged)
    Q_PROPERTY(QStringList loaderVersionOptions READ LoaderVersionOptions NOTIFY downloadChanged)
    Q_PROPERTY(bool loaderVersionsBusy READ LoaderVersionsBusy NOTIFY downloadChanged)
    Q_PROPERTY(QString loaderVersionsError READ LoaderVersionsError NOTIFY downloadChanged)
    Q_PROPERTY(QVariantList tasks READ Tasks NOTIFY tasksChanged)
    Q_PROPERTY(int activeTasks READ ActiveTasks NOTIFY tasksChanged)
    Q_PROPERTY(bool launchRepairing READ LaunchRepairing NOTIFY changed)
    Q_PROPERTY(bool launchCancelling READ LaunchCancelling NOTIFY changed)
    Q_PROPERTY(double launchProgress READ LaunchProgress NOTIFY changed)
    Q_PROPERTY(bool launchFailed READ LaunchFailed NOTIFY changed)
    Q_PROPERTY(QStringList javaOptions READ JavaOptions NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ SelectedIndex NOTIFY changed)
    Q_PROPERTY(QString installingVersion READ InstallingVersion NOTIFY changed)
    Q_PROPERTY(int memoryMb READ MemoryMb NOTIFY changed)
    Q_PROPERTY(QString gcMode READ GcMode NOTIFY changed)
    Q_PROPERTY(QString jvmArgs READ JvmArgs NOTIFY changed)
    Q_PROPERTY(QString coreSummary READ CoreSummary NOTIFY changed)
    Q_PROPERTY(QString maintenanceReport READ MaintenanceReport NOTIFY changed)

public:
    Bridge(Launch& launch, QString account_name, int memory_mb,
           std::function<bool()> server_running = {}, QObject* parent = nullptr);

    QStringList Versions() const { return versions_.Names(); }
    QString Selected() const { return versions_.Selected(); }
    QString SelectedName() const { return versions_.SelectedName(); }
    QString Details() const { return versions_.Details(); }
    QString JavaInfo() const { return versions_.JavaInfo(); }
    QString Status() const { return status_; }
    bool Busy() const { return busy_; }
    QString GameDir() const { return QString::fromStdString(launch_.Versions().GameDir()); }
    QStringList Javas() const { return versions_.Javas(); }
    QString Loader() const { return versions_.Loader(); }
    QString BackgroundUrl() const { return BridgeBackground::Url(); }
    bool HasCustomBackground() const { return BridgeBackground::Has(); }
    QVariantList SourceRecords() const;
    QVariantList SourceKindOptions() const;
    QStringList ContentResults() const { return downloads_.ContentResults(); }
    QStringList ContentNames() const { return downloads_.ContentNames(); }
    QStringList ContentIcons() const { return downloads_.ContentIcons(); }
    QStringList ContentDescriptions() const { return downloads_.ContentDescriptions(); }
    QString DownloadStatus() const { return downloads_.Status(); }
    bool DownloadBusy() const { return downloads_.Busy(); }
    QStringList ContentSources() const { return downloads_.ContentSources(); }
    QStringList ReleaseVersions() const { return downloads_.ReleaseVersions(); }
    QStringList SnapshotVersions() const { return downloads_.SnapshotVersions(); }
    QStringList AprilVersions() const { return downloads_.AprilVersions(); }
    double DownloadProgress() const { return downloads_.Progress(); }
    bool DownloadFailed() const { return downloads_.Failed(); }
    QStringList LoaderVersionOptions() const { return downloads_.LoaderVersions(); }
    bool LoaderVersionsBusy() const { return downloads_.LoaderVersionsBusy(); }
    QString LoaderVersionsError() const { return downloads_.LoaderVersionsError(); }
    QVariantList Tasks() const { return tasks_.Snapshots(); }
    int ActiveTasks() const { return tasks_.Active(); }
    bool LaunchRepairing() const { return repairing_; }
    bool LaunchCancelling() const { return cancelling_; }
    double LaunchProgress() const { return repair_progress_; }
    bool LaunchFailed() const { return repair_failed_; }
    QStringList JavaOptions() const { return versions_.JavaOptions(); }
    int SelectedIndex() const { return versions_.SelectedIndex(); }
    QString InstallingVersion() const { return downloads_.InstallingVersion(); }
    int MemoryMb() const { return memory_mb_; }
    QString GcMode() const { return gc_mode_; }
    QString JvmArgs() const { return jvm_args_; }
    QString CoreSummary() const { return core_summary_; }
    QString MaintenanceReport() const { return maintenance_report_; }

public slots:
    Q_INVOKABLE void selectVersionAt(int index) {
        if (versions_.SelectAt(index)) emit changed();
    }
    Q_INVOKABLE void openVersionFolderAt(int index) { versions_.OpenFolderAt(index); }
    Q_INVOKABLE void deleteVersionAt(int index);
    Q_INVOKABLE void renameVersionAt(int index, const QString& name);

    // 核心维护：dry-run 检查、正式迁移、回滚、清理孤立核心。
    Q_INVOKABLE void checkCores();
    Q_INVOKABLE void migrateCores();
    Q_INVOKABLE void rollbackCoreMigration();
    Q_INVOKABLE void cleanOrphanCores();
    Q_INVOKABLE void applyLaunchOptions(int memory_mb, const QString& gc, const QString& jvm_args);
    Q_INVOKABLE void launchGame();
    Q_INVOKABLE void cancelLaunchRepair();
    Q_INVOKABLE void installJava(int major);
    Q_INVOKABLE bool canUninstallJava(int index) const;
    Q_INVOKABLE void uninstallJava(int index);
    Q_INVOKABLE QString selectedMinecraftVersion() const {
        return QString::fromStdString(versions_.SelectedMinecraftVersion());
    }
    Q_INVOKABLE bool importMod(const QString& source, const QString& gameVersion,
                               const QString& loader, const QString& modVersion);
    Q_INVOKABLE void loadGameVersions() { downloads_.LoadGameVersions(); }
    Q_INVOKABLE void searchContent(const QString& query, const QString& kind);
    Q_INVOKABLE void installContent(int index);
    Q_INVOKABLE void selectContentSource(int index) { downloads_.SelectSource(index); }
    Q_INVOKABLE void installGame(int kind, int index, const QString& loader) {
        downloads_.InstallGame(kind, index, loader);
    }
    Q_INVOKABLE void installGameWithLoader(int kind, int index, const QString& loader,
                                           const QString& loaderVersion) {
        downloads_.InstallGame(kind, index, loader, loaderVersion);
    }
    Q_INVOKABLE void loadLoaderVersions(const QString& mcVersion, const QString& loader) {
        downloads_.LoadLoaderVersions(mcVersion, loader);
    }
    Q_INVOKABLE void pauseTask(qulonglong id);
    Q_INVOKABLE void resumeTask(qulonglong id);
    Q_INVOKABLE void cancelTask(qulonglong id);
    Q_INVOKABLE void retryTask(qulonglong id);
    Q_INVOKABLE void installServer(int kind, int index, const QString& loader) {
        downloads_.InstallServer(kind, index, loader);
    }

    // 当前版本的 Mod：仓库 / 已安装
    Q_INVOKABLE QVariantList installedMods();
    Q_INVOKABLE QVariantList repositoryMods();
    Q_INVOKABLE bool installRepositoryMod(int index);
    Q_INVOKABLE bool removeInstalledMod(int index);

    // 背景图
    Q_INVOKABLE void setBackground(const QString& path);
    Q_INVOKABLE void resetBackground();

    // 软件源
    Q_INVOKABLE void saveSource(int index, const QString& name, const QString& url);
    Q_INVOKABLE void addSource(const QString& kind, const QString& name, const QString& url);
    Q_INVOKABLE void removeSource(int index);
    Q_INVOKABLE void resetSources();

    Q_INVOKABLE void selectJava(int index) {
        versions_.SelectJava(index);
        emit changed();
    }

signals:
    void changed();
    void downloadChanged();
    void sourcesChanged();
    void tasksChanged();

private:
    void RefreshStatus();
    void SaveSources();
    bool ApplyModResult(bool ok, const QString& message);
    void RefreshCoreSummary();
    bool MaintenanceBlocked(QString& reason) const;
    void SetMaintenanceReport(const std::vector<std::string>& lines, const std::string& error,
                              const QString& done_text);

    Launch& launch_;
    QString account_name_;
    std::function<bool()> server_running_;
    int memory_mb_ = 4096;
    QString gc_mode_ = QStringLiteral("自动");
    QString jvm_args_;
    QString status_ = QStringLiteral("空闲");
    QString core_summary_;
    QString maintenance_report_;
    bool busy_ = false;
    bool game_started_ = false;
    QTimer status_timer_;

    BridgeVersions versions_;
    BridgeTasks tasks_;
    BridgeDownloads downloads_;
    BridgeMods mods_;

    bool repairing_ = false;
    bool cancelling_ = false;
    double repair_progress_ = 0.0;
    bool repair_failed_ = false;
    std::atomic<bool> cancel_requested_{false};
};
