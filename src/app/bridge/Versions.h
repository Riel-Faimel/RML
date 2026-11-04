// 版本视图：版本列表、选中版本详情、Java 列表与选择。
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <string>

#include "app/directory/Directory.h"
#include "app/launch/Launch.h"

class BridgeVersions {
public:
    BridgeVersions(Launch& launch, QObject* context);

    const QStringList& Names() const { return names_; }
    QString Selected() const { return selected_; }
    QString SelectedName() const;
    QString Details() const { return details_; }
    QString JavaInfo() const { return java_info_; }
    QString Loader() const { return loader_; }
    const QStringList& Javas() const { return javas_; }
    const QStringList& JavaOptions() const { return java_options_; }
    const QString& JavaPathOverride() const { return java_path_override_; }
    int SelectedIndex() const { return selected_index_; }

    bool SelectAt(int index);
    void SelectInstalled(const std::string& id);
    void OpenFolderAt(int index);
    // 删入口不删核心；失败返回中文错误。
    bool DeleteAt(int index, std::string& error);
    void RenameAt(int index, const QString& name);
    void SelectJava(int index);
    bool CanUninstallJava(int index) const;
    bool UninstallJava(int index, std::string& error);
    void InstallJava(int major, std::function<void(bool, const QString&)> done);
    void RefreshJavas();
    void Refresh();
    void UpdateDetails();

    const Directory::VersionEntry* SelectedEntry() const;
    std::string SelectedMinecraftVersion() const;
    std::string SelectedContentLoader() const;
    std::string VersionIdAt(int index) const;

private:
    Launch& launch_;
    QObject* context_;
    QStringList names_;
    QStringList ids_;
    QString selected_;
    int selected_index_ = -1;
    QString details_;
    QString java_info_;
    QString loader_ = QStringLiteral("原版");
    QStringList javas_;
    QStringList java_options_;
    QStringList java_paths_;
    QString java_path_override_;
};
