#include "app/bridge/Background.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QUrl>

#include "app/directory/Directory.h"

namespace {

QString FromStd(const std::string& text) { return QString::fromStdString(text); }

// 背景图带时间戳，换图时 URL 会变，QML 的 Image 才会重新加载
QString CustomBackgroundFile() {
    QDir dir(FromStd(Directory::DataRoot()));
    const QStringList files =
        dir.entryList({QStringLiteral("background*.png")}, QDir::Files, QDir::Time);
    for (const QString& name : files) {
        const QString path = dir.filePath(name);
        if (QFile::exists(path)) return path;
    }
    return {};
}

void RemoveCustomBackgrounds(const QString& keep = QString()) {
    QDir dir(FromStd(Directory::DataRoot()));
    for (const QString& name : dir.entryList({QStringLiteral("background*.png")}, QDir::Files)) {
        const QString path = dir.filePath(name);
        if (!keep.isEmpty() && path == keep) continue;
        QFile::remove(path);
    }
}

}  // namespace

QString BridgeBackground::Url() {
    const QString custom = CustomBackgroundFile();
    if (!custom.isEmpty()) return QUrl::fromLocalFile(custom).toString();

    // 部署后优先用 exe 旁边的 qml/assets
    const QString local =
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/assets/background.png");
    if (QFile::exists(local)) return QUrl::fromLocalFile(local).toString();

    return QUrl::fromLocalFile(QStringLiteral(RML_QML_DIR "/assets/background.png")).toString();
}

bool BridgeBackground::Has() { return !CustomBackgroundFile().isEmpty(); }

bool BridgeBackground::Set(const QString& path, QString& error) {
    // QML 的 FileDialog 给的是 file:/// URL，这里统一成本地路径
    QString local = path;
    const QUrl url(path);
    if (url.isLocalFile()) local = url.toLocalFile();

    const QImage image(local);
    if (image.isNull()) {
        error = QStringLiteral("背景图读取失败：%1").arg(local);
        return false;
    }

    const QString data_root = FromStd(Directory::DataRoot());
    QDir().mkpath(data_root);
    const QString target =
        data_root + QStringLiteral("/background-%1.png").arg(QDateTime::currentMSecsSinceEpoch());
    if (!image.save(target, "PNG")) {
        error = QStringLiteral("背景图保存失败：%1").arg(target);
        return false;
    }

    RemoveCustomBackgrounds(target);
    return true;
}

void BridgeBackground::Reset() { RemoveCustomBackgrounds(); }


