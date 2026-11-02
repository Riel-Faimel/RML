// 应用层：把启动模块暴露给 QML。不属于任何模块。
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "include/AccountProvider.h"
#include "app/launch/Launch.h"

class Bridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList versions READ Versions NOTIFY changed)
    Q_PROPERTY(QString selected READ Selected NOTIFY changed)
    Q_PROPERTY(QString details READ Details NOTIFY changed)
    Q_PROPERTY(QString javaInfo READ JavaInfo NOTIFY changed)
    Q_PROPERTY(QString status READ Status NOTIFY changed)
    Q_PROPERTY(bool busy READ Busy NOTIFY changed)
    Q_PROPERTY(QString gameDir READ GameDir CONSTANT)
    Q_PROPERTY(QString accountName READ AccountName NOTIFY changed)
    Q_PROPERTY(QStringList javas READ Javas NOTIFY changed)
    Q_PROPERTY(QString loader READ Loader NOTIFY changed)
    Q_PROPERTY(QString backgroundUrl READ BackgroundUrl CONSTANT)
    Q_PROPERTY(QStringList contentResults READ ContentResults NOTIFY downloadChanged)
    Q_PROPERTY(QString downloadStatus READ DownloadStatus NOTIFY downloadChanged)
    Q_PROPERTY(bool downloadBusy READ DownloadBusy NOTIFY downloadChanged)
    Q_PROPERTY(QStringList contentSources READ ContentSources NOTIFY downloadChanged)
    Q_PROPERTY(QStringList releaseVersions READ ReleaseVersions NOTIFY downloadChanged)
    Q_PROPERTY(QStringList snapshotVersions READ SnapshotVersions NOTIFY downloadChanged)
    Q_PROPERTY(QStringList aprilVersions READ AprilVersions NOTIFY downloadChanged)
    Q_PROPERTY(double downloadProgress READ DownloadProgress NOTIFY downloadChanged)
    Q_PROPERTY(QStringList javaOptions READ JavaOptions NOTIFY changed)

public:
    Bridge(Launch& launch, QString account_name, int memory_mb, QObject* parent = nullptr);

    QStringList Versions() const { return versions_; }
    QString Selected() const { return selected_; }
    QString Details() const { return details_; }
    QString JavaInfo() const { return java_info_; }
    QString Status() const { return status_; }
    bool Busy() const { return busy_; }
    QString GameDir() const;
    QString AccountName() const { return account_name_; }
    QStringList Javas() const { return javas_; }
    QString Loader() const { return loader_; }
    QString BackgroundUrl() const;
    QStringList ContentResults() const { return content_results_; }
    QString DownloadStatus() const { return download_status_; }
    bool DownloadBusy() const { return download_busy_; }
    QStringList ContentSources() const { return content_sources_; }
    QStringList ReleaseVersions() const { return release_versions_; }
    QStringList SnapshotVersions() const { return snapshot_versions_; }
    QStringList AprilVersions() const { return april_versions_; }
    double DownloadProgress() const { return download_progress_; }
    QStringList JavaOptions() const { return java_options_; }

public slots:
    Q_INVOKABLE void selectVersion(const QString& id);
    Q_INVOKABLE void launchGame();
    Q_INVOKABLE void installJava(int major);
    Q_INVOKABLE void repair();
    Q_INVOKABLE void loadGameVersions();
    Q_INVOKABLE void searchContent(const QString& query, const QString& kind);
    Q_INVOKABLE void installContent(int index);
    Q_INVOKABLE void selectContentSource(int index);
    Q_INVOKABLE void installGame(int kind, int index);
    Q_INVOKABLE void selectJava(int index);

signals:
    void changed();
    void downloadChanged();

private:
    void RefreshStatus();
    void UpdateDetails();
    void RefreshJavas();
    void RefreshVersionList();
    ContentSource* selectedSource();

    Launch& launch_;
    QString account_name_;
    int memory_mb_ = 4096;
    QStringList versions_;
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
