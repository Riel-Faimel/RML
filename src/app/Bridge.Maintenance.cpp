#include "app/Bridge.h"

#include <string>
#include <vector>

#include "app/directory/Directory.h"

void Bridge::deleteVersionAt(int index) {
    if (launch_.GameStarter().IsRunning()) {
        status_ = QStringLiteral("游戏正在运行，请先关闭再删除版本");
        emit changed();
        return;
    }

    const std::string id = versions_.VersionIdAt(index);
    std::string core;
    for (const auto& entry : launch_.Versions().Entries()) {
        if (entry.id == id) {
            core = entry.core;
            break;
        }
    }

    std::string error;
    if (!versions_.DeleteAt(index, error)) {
        status_ = QStringLiteral("删除版本失败：%1").arg(QString::fromStdString(error));
        emit changed();
        return;
    }

    RefreshCoreSummary();
    status_ = QStringLiteral("已删除版本入口：%1").arg(QString::fromStdString(id));
    const auto* found = launch_.Versions().Paths().FindCore(core);
    if (found != nullptr && found->Orphan()) {
        status_ += QStringLiteral("；核心 %1 已无引用，可在设置页清理")
                       .arg(QString::fromStdString(core));
    }
    emit changed();
}

void Bridge::RefreshCoreSummary() {
    int cores = 0;
    int orphans = 0;
    for (const auto& core : launch_.Versions().Paths().Cores()) {
        if (!core.valid) continue;
        ++cores;
        if (core.Orphan()) ++orphans;
    }
    core_summary_ = QStringLiteral("核心 %1 个 · 孤立 %2 个").arg(cores).arg(orphans);
}

bool Bridge::MaintenanceBlocked(QString& reason) const {
    if (launch_.GameStarter().IsRunning()) {
        reason = QStringLiteral("游戏正在运行，请先关闭再做核心维护");
        return true;
    }
    if (server_running_ && server_running_()) {
        reason = QStringLiteral("服务器正在运行，请先停止再做核心维护");
        return true;
    }
    return false;
}

void Bridge::SetMaintenanceReport(const std::vector<std::string>& lines,
                                  const std::string& error, const QString& done_text) {
    QStringList text;
    for (const auto& line : lines) text << QString::fromStdString(line);
    if (!error.empty()) text << QStringLiteral("错误：%1").arg(QString::fromStdString(error));
    maintenance_report_ = text.join(QLatin1Char('\n'));
    if (maintenance_report_.size() > 8000) {
        maintenance_report_ = maintenance_report_.left(8000) + QStringLiteral("\n…");
    }
    status_ = error.empty() ? done_text
                            : QStringLiteral("核心维护失败：%1").arg(QString::fromStdString(error));
}

void Bridge::checkCores() {
    QString reason;
    if (MaintenanceBlocked(reason)) {
        status_ = reason;
        emit changed();
        return;
    }
    Directory paths(launch_.Versions().GameDir());
    Directory::MigrationReport report;
    paths.MigrateCores(true, report);
    SetMaintenanceReport(report.lines, report.error,
                         report.ok ? QStringLiteral("核心检查完成（未改动文件）")
                                   : QStringLiteral("核心检查失败"));
    emit changed();
}

void Bridge::migrateCores() {
    QString reason;
    if (MaintenanceBlocked(reason)) {
        status_ = reason;
        emit changed();
        return;
    }
    Directory paths(launch_.Versions().GameDir());
    Directory::MigrationReport report;
    paths.MigrateCores(false, report);
    if (report.ok) {
        launch_.Versions().Refresh();
        versions_.Refresh();
    }
    RefreshCoreSummary();
    SetMaintenanceReport(report.lines, report.error,
                         report.ok ? QStringLiteral("核心迁移完成") : QStringLiteral("核心迁移失败"));
    emit changed();
}

void Bridge::rollbackCoreMigration() {
    QString reason;
    if (MaintenanceBlocked(reason)) {
        status_ = reason;
        emit changed();
        return;
    }
    Directory paths(launch_.Versions().GameDir());
    std::string report;
    std::string error;
    paths.RollbackLastMigration(error, report);
    if (error.empty()) {
        launch_.Versions().Refresh();
        versions_.Refresh();
    }
    RefreshCoreSummary();
    maintenance_report_ = QString::fromStdString(report.empty() ? error : report);
    status_ = error.empty() ? QStringLiteral("已回滚上次核心迁移")
                            : QStringLiteral("回滚失败：%1").arg(QString::fromStdString(error));
    emit changed();
}

void Bridge::cleanOrphanCores() {
    QString reason;
    if (MaintenanceBlocked(reason)) {
        status_ = reason;
        emit changed();
        return;
    }
    launch_.Versions().Refresh();
    Directory paths(launch_.Versions().GameDir());
    std::string error;
    const int removed = paths.RemoveOrphanCores(error);
    launch_.Versions().Refresh();
    RefreshCoreSummary();

    QString text = QStringLiteral("清理孤立核心 %1 个").arg(removed);
    if (!error.empty()) text += QStringLiteral("\n%1").arg(QString::fromStdString(error));
    maintenance_report_ = text;
    status_ = error.empty() ? text
                            : QStringLiteral("部分孤立核心清理失败：%1")
                                  .arg(QString::fromStdString(error));
    emit changed();
}
