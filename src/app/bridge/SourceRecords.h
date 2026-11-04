// 软件源记录的界面映射：记录 → QVariantList，增删改查。
#pragma once

#include <QString>
#include <QVariantList>

#include "app/download/VersionSource/Sources/Sources.h"

class BridgeSourceRecords {
public:
    static QVariantList KindOptions();
    static QVariantList Records(const download::versionsource::Sources& sources);

    static bool Update(download::versionsource::Sources& sources, int index, const QString& name,
                       const QString& url);
    static bool Add(download::versionsource::Sources& sources, const QString& kind,
                    const QString& name, const QString& url, QString& error);
    static bool Remove(download::versionsource::Sources& sources, int index);
    static void Reset(download::versionsource::Sources& sources);
    static bool Save(download::versionsource::Sources& sources, QString& error);
};
