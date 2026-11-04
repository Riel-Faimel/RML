#include "app/bridge/Tasks.h"

#include <QString>
#include <QVariantMap>

#include <string>
#include <utility>

BridgeTasks::BridgeTasks(Notify changed) : changed_(std::move(changed)) {
    QObject::connect(&timer_, &QTimer::timeout, [this] { Tick(); });
    timer_.start(500);
}

void BridgeTasks::Tick() {
    std::string signature;
    for (const auto& task : tasks_.Snapshots()) {
        signature += std::to_string(task.id);
        signature += '|';
        signature += task.status;
        signature += '|';
        signature += task.stage;
        signature += '|';
        signature += task.error;
        signature += '|';
        signature += std::to_string(task.progress);
        signature += '\n';
    }
    // 列表没有变化就不刷新模型，否则 ListView 每 500ms 被重置，滚动位置会回到顶部。
    if (signature == last_signature_) return;
    last_signature_ = std::move(signature);
    changed_();
}

QVariantList BridgeTasks::Snapshots() const {
    QVariantList out;
    for (const auto& task : tasks_.Snapshots()) {
        QVariantMap item;
        item["id"] = static_cast<qulonglong>(task.id);
        item["title"] = QString::fromStdString(task.title);
        item["stage"] = QString::fromStdString(task.stage);
        item["status"] = QString::fromStdString(task.status);
        item["error"] = QString::fromStdString(task.error);
        item["progress"] = task.progress;
        out.append(item);
    }
    return out;
}

int BridgeTasks::Active() const {
    int count = 0;
    for (const auto& task : tasks_.Snapshots()) {
        if (task.status == "queued" || task.status == "running" || task.status == "paused") ++count;
    }
    return count;
}

void BridgeTasks::Pause(qulonglong id) {
    tasks_.Pause(id);
    last_signature_.clear();
    changed_();
}

void BridgeTasks::Resume(qulonglong id) {
    tasks_.Resume(id);
    last_signature_.clear();
    changed_();
}

void BridgeTasks::Cancel(qulonglong id) {
    tasks_.Cancel(id);
    last_signature_.clear();
    changed_();
}

void BridgeTasks::Retry(qulonglong id) {
    tasks_.Retry(id);
    last_signature_.clear();
    changed_();
}
