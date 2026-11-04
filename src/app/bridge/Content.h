// 内容源：Mod/资源包/光影的搜索与安装。
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <string>

#include "app/bridge/DownloadState.h"
#include "app/launch/Launch.h"
#include "app/nettask/NetTask.h"
#include "include/ContentSource.h"

class BridgeContent {
public:
    using Notify = std::function<void()>;

    BridgeContent(Launch& launch, NetTask& tasks, QObject* context, BridgeDownloadState& state,
                  Notify download_changed);

    const QStringList& Results() const { return results_; }
    const QStringList& Sources() const { return sources_; }

    void SelectSource(int index);
    void Search(const QString& query, const QString& kind, const std::string& mc_version,
                const std::string& loader);
    void Install(int index, const std::string& mc_version, const std::string& loader,
                 const std::string& repo_dir, const std::string& instance_dir);

private:
    ContentSource* SelectedSource();

    Launch& launch_;
    NetTask& tasks_;
    QObject* context_;
    BridgeDownloadState& state_;
    Notify download_changed_;

    QStringList results_;
    QStringList names_;
    QStringList ids_;
    QStringList sources_;
    int source_index_ = 0;
    QString kind_ = QStringLiteral("mod");
};
