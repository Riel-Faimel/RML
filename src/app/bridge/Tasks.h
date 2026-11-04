// 任务视图：把 NetTask 的快照和操作暴露给 Bridge。
#pragma once

#include <QTimer>
#include <QVariantList>

#include <functional>

#include "app/nettask/NetTask.h"

class BridgeTasks {
public:
    using Notify = std::function<void()>;

    explicit BridgeTasks(Notify changed);

    NetTask& Sink() { return tasks_; }

    QVariantList Snapshots() const;
    int Active() const;
    void Pause(qulonglong id);
    void Resume(qulonglong id);
    void Cancel(qulonglong id);
    void Retry(qulonglong id);

private:
    void Tick();

    NetTask tasks_;
    QTimer timer_;
    Notify changed_;
    std::string last_signature_;
};
