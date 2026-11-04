// 自定义背景图：复制到数据目录，换图时用时间戳让 QML 重新加载。
#pragma once

#include <QString>

class BridgeBackground {
public:
    static QString Url();
    static bool Has();

    static bool Set(const QString& path, QString& error);
    static void Reset();
};
