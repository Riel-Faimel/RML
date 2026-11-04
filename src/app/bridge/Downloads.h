// 下载视图：版本清单、安装游戏/服务端；内容搜索交给 BridgeContent。
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <string>

#include "app/bridge/Content.h"
#include "app/bridge/DownloadState.h"
#include "app/launch/Launch.h"
#include "app/nettask/NetTask.h"

class BridgeDownloads {
public:
    using Notify = std::function<void()>;
    using InstalledHandler = std::function<void(const std::string&)>;

    BridgeDownloads(Launch& launch, NetTask& tasks, QObject* context, Notify download_changed,
                    Notify changed, InstalledHandler installed);

    const QStringList& ContentResults() const { return content_.Results(); }
    QString Status() const { return state_.status; }
    bool Busy() const { return state_.busy; }
    const QStringList& ContentSources() const { return content_.Sources(); }
    const QStringList& ReleaseVersions() const { return release_versions_; }
    const QStringList& SnapshotVersions() const { return snapshot_versions_; }
    const QStringList& AprilVersions() const { return april_versions_; }
    double Progress() const { return state_.progress; }
    bool Failed() const { return state_.failed; }
    QString InstallingVersion() const { return state_.installing_version; }
    const QStringList& LoaderVersions() const { return loader_versions_; }
    bool LoaderVersionsBusy() const { return loader_versions_busy_; }
    QString LoaderVersionsError() const { return loader_versions_error_; }

    void LoadGameVersions();
    void LoadLoaderVersions(const QString& mc_version, const QString& loader);
    void InstallGame(int kind, int index, const QString& loader,
                     const QString& loader_version = QString());
    void InstallServer(int kind, int index, const QString& loader);

    void SearchContent(const QString& query, const QString& kind, const std::string& mc_version,
                       const std::string& loader) {
        content_.Search(query, kind, mc_version, loader);
    }
    void InstallContent(int index, const std::string& mc_version, const std::string& loader,
                        const std::string& instance_dir) {
        content_.Install(index, mc_version, loader, instance_dir);
    }
    void SelectSource(int index) { content_.SelectSource(index); }

    // 外部组件（如 Mod 管理）写回一次状态
    void SetStatus(const QString& text);

private:
    const QStringList* VersionIdsFor(int kind) const;

    Launch& launch_;
    NetTask& tasks_;
    QObject* context_;
    Notify download_changed_;
    Notify changed_;
    InstalledHandler installed_;

    BridgeDownloadState state_;
    BridgeContent content_;

    QStringList release_versions_;
    QStringList snapshot_versions_;
    QStringList april_versions_;
    QStringList release_ids_;
    QStringList snapshot_ids_;
    QStringList april_ids_;
    QStringList loader_versions_;
    bool loader_versions_busy_ = false;
    QString loader_versions_error_;
};
