#include "app/bridge/SourceRecords.h"

#include <QVariantMap>

#include "app/bridge/Labels.h"

namespace {

QString FromStd(const std::string& text) { return QString::fromStdString(text); }

}  // namespace

QVariantList BridgeSourceRecords::KindOptions() {
    QVariantList out;
    for (const auto& kind : download::versionsource::Sources::Kinds()) {
        QVariantMap map;
        map["kind"] = FromStd(kind);
        map["label"] = BridgeLabels::SourceKindLabel(kind);
        out.append(map);
    }
    return out;
}

QVariantList BridgeSourceRecords::Records(const download::versionsource::Sources& sources) {
    QVariantList out;
    for (const auto& record : sources.Records()) {
        QVariantMap map;
        map["kind"] = FromStd(record.kind);
        map["kindLabel"] = BridgeLabels::SourceKindLabel(record.kind);
        map["name"] = FromStd(record.name);
        map["url"] = FromStd(record.url);
        out.append(map);
    }
    return out;
}

bool BridgeSourceRecords::Update(download::versionsource::Sources& sources, int index,
                           const QString& name, const QString& url) {
    if (index < 0 || static_cast<size_t>(index) >= sources.Records().size()) return false;
    sources.Update(static_cast<size_t>(index), name.trimmed().toStdString(),
                   url.trimmed().toStdString());
    return true;
}

bool BridgeSourceRecords::Add(download::versionsource::Sources& sources, const QString& kind,
                        const QString& name, const QString& url, QString& error) {
    if (!download::versionsource::Sources::KnownKind(kind.toStdString())) {
        error = QStringLiteral("不认识的软件源类型：%1").arg(kind);
        return false;
    }
    sources.Add(kind.toStdString(), name.trimmed().toStdString(), url.trimmed().toStdString());
    return true;
}

bool BridgeSourceRecords::Remove(download::versionsource::Sources& sources, int index) {
    if (index < 0 || static_cast<size_t>(index) >= sources.Records().size()) return false;
    sources.Remove(static_cast<size_t>(index));
    return true;
}

void BridgeSourceRecords::Reset(download::versionsource::Sources& sources) { sources.ResetDefaults(); }

bool BridgeSourceRecords::Save(download::versionsource::Sources& sources, QString& error) {
    std::string text;
    if (!sources.Save(text)) {
        error = FromStd(text);
        return false;
    }
    return true;
}


