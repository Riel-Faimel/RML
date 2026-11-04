// 下载视图的共享状态：安装/搜索/下载进度都写这一份。
#pragma once

#include <QString>

struct BridgeDownloadState {
    QString status = QStringLiteral("选一个分类开始");
    bool busy = false;
    double progress = 0.0;
    bool failed = false;
    QString installing_version;
};
