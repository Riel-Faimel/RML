// 应用层：把启动模块暴露给 QML。不属于任何模块。
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include "include/AccountProvider.h"
#include "app/launch/Launch.h"

class Bridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList versions READ Versions NOTIFY changed)
    Q_PROPERTY(QString selected READ Selected NOTIFY changed)
    Q_PROPERTY(QString selectedName READ SelectedName NOTIFY changed)
    Q_PROPERTY(QString details READ Details NOTIFY changed)
    Q_PROPERTY(QString javaInfo READ JavaInfo NOTIFY changed)
    Q_PROPERTY(QString status READ Status NOTIFY changed)
    Q_PROPERTY(bool busy READ Busy NOTIFY changed)
    Q_PROPERTY(QString gameDir READ GameDir CONSTANT)
    Q_PROPERTY(QString accountName READ AccountName NOTIFY changed)
    Q_PROPERTY(QStringList javas READ Javas NOTIFY changed)
    Q_PROPERTY(QString loader READ Loader NOTIFY changed)
    Q_PROPERTY(QString backgroundUrl READ BackgroundUrl NOTIFY changed)
    Q_PROPERTY(bool hasCustomBackground READ HasCustomBackground NOTIFY changed)
    Q_PROPERTY(QVariantList sourceRecords READ SourceRecords NOTIFY sourcesChanged)
    Q_PROPERTY(QVariantList sourceKindOptions READ SourceKindOptions CONSTANT)
    Q_PROPERTY(QStringList contentResults READ ContentResults NOTIFY downloadChanged)
    Q_PROPERTY(QString downloadStatus READ DownloadStatus NOTIFY downloadChanged)
    Q_PROPERTY(bool downloadBusy READ DownloadBusy NOTIFY downloadChanged)
    Q_PROPERTY(QStringList contentSources READ ContentSources NOTIFY downloadChanged)
    Q_PROPERTY(QStringList releaseVersions READ ReleaseVersions NOTIFY downloadChanged)
    Q_PROPERTY(QStringList snapshotVersions READ SnapshotVersions NOTIFY downloadChanged)
    Q_PROPERTY(QStringList aprilVersions READ AprilVersions NOTIFY downloadChanged)
    Q_PROPERTY(double downloadProgress READ DownloadProgress NOTIFY downloadChanged)
    Q_PROPERTY(QStringList javaOptions READ JavaOptions NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ SelectedIndex NOTIFY changed)
    Q_PROPERTY(QString installingVersion READ InstallingVersion NOTIFY changed)
    Q_PROPERTY(int memoryMb READ MemoryMb NOTIFY changed)
    Q_PROPERTY(QString gcMode READ GcMode NOTIFY changed)
    Q_PROPERTY(QString jvmArgs READ JvmArgs NOTIFY changed)

public:
    Bridge(Launch& launch, QString account_name, int memory_mb, QObject* parent = nullptr);

    QStringList Versions() const { return versions_; }
    QString Selected() const { return selected_; }
    QString SelectedName() const;
    QString Details() const { return details_; }
    QString JavaInfo() const { return java_info_; }
    QString Status() const { return status_; }
    bool Busy() const { return busy_; }
    QString GameDir() const;
    QString AccountName() const { return account_name_; }
    QStringList Javas() const { return javas_; }
    QString Loader() const { return loader_; }
    QString BackgroundUrl() const;
    bool HasCustomBackground() const;
    QVariantList SourceRecords() const;
    QVariantList SourceKindOptions() const;
    QStringList ContentResults() const { return content_results_; }
    QString DownloadStatus() const { return download_status_; }
    bool DownloadBusy() const { return download_busy_; }
    QStringList ContentSources() const { return content_sources_; }
    QStringList ReleaseVersions() const { return release_versions_; }
    QStringList SnapshotVersions() const { return snapshot_versions_; }
    QStringList AprilVersions() const { return april_versions_; }
    double DownloadProgress() const { return download_progress_; }
    QStringList JavaOptions() const { return java_options_; }
    int SelectedIndex() const { return selected_index_; }
    QString InstallingVersion() const { return installing_version_; }
    int MemoryMb() const { return memory_mb_; }
    QString GcMode() const { return gc_mode_; }
    QString JvmArgs() const { return jvm_args_; }

public slots:
    Q_INVOKABLE void selectVersionAt(int index);
    Q_INVOKABLE void openVersionFolderAt(int index);
    Q_INVOKABLE void deleteVersionAt(int index);
    Q_INVOKABLE void renameVersionAt(int index, const QString& name);
    Q_INVOKABLE void selectVersion(const QString& id);
    Q_INVOKABLE void applyLaunchOptions(int memory_mb, const QString& gc, const QString& jvm_args);
    Q_INVOKABLE void launchGame();
    Q_INVOKABLE void installJava(int major);
    Q_INVOKABLE void loadGameVersions();
    Q_INVOKABLE void searchContent(const QString& query, const QString& kind);
    Q_INVOKABLE void installContent(int index);
    Q_INVOKABLE void selectContentSource(int index);
    Q_INVOKABLE void installGame(int kind, int index, const QString& loader);
    Q_INVOKABLE void installServer(int kind, int index, const QString& loader);

    // 背景图
    Q_INVOKABLE void setBackground(const QString& path);
    Q_INVOKABLE void resetBackground();

    // 软件源
    Q_INVOKABLE void saveSource(int index, const QString& name, const QString& url);
    Q_INVOKABLE void addSource(const QString& kind, const QString& name, const QString& url);
    Q_INVOKABLE void removeSource(int index);
    Q_INVOKABLE void resetSources();

    Q_INVOKABLE void selectJava(int index);

signals:
    void changed();
    void downloadChanged();
    void sourcesChanged();

private:
    void RefreshStatus();
    void UpdateDetails();
    void RefreshJavas();
    void RefreshVersionList();
    void SaveSources();
    std::string versionIdAt(int index) const;
    ContentSource* selectedSource();

    Launch& launch_;
    QString account_name_;
    int memory_mb_ = 4096;
    QStringList versions_;
    QStringList version_ids_;
    int selected_index_ = -1;
    QString installing_version_;
    QString gc_mode_ = QStringLiteral("自动");
    QString jvm_args_;
    QString selected_;
    QString details_;
    QString java_info_;
    QStringList javas_;
    QStringList java_options_;
    QStringList java_paths_;
    QString java_path_override_;
    QString loader_ = QStringLiteral("原版");
    QString status_ = QStringLiteral("空闲");
    bool busy_ = false;
    QTimer status_timer_;

    QStringList content_results_;
    QStringList content_ids_;
    QStringList content_sources_;
    QStringList release_versions_, snapshot_versions_, april_versions_;
    QStringList release_ids_, snapshot_ids_, april_ids_;
    int content_source_ = 0;
    QString content_kind_ = QStringLiteral("mod");
    QString download_status_ = QStringLiteral("选一个分类开始");
    bool download_busy_ = false;
    double download_progress_ = 0.0;
};
