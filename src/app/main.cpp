#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QString>
#include <QUrl>

#include <string>

#include "app/Bridge.h"
#include "app/directory/Directory.h"
#include "app/ServerBridge.h"
#include "app/launch/Launch.h"
#include "app/server/Server.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

#ifdef RML_DEBUG
#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#endif

namespace {

// 所有自己的数据都在 %APPDATA%\RML 下，路径统一由 Directory 决定
QString DataDir() { return QString::fromStdString(Directory::DataRoot()); }

QString LogDir() { return QString::fromStdString(Directory::LauncherLogsRoot()); }

#ifdef RML_DEBUG
void DebugMessageHandler(QtMsgType type, const QMessageLogContext&, const QString& message) {
    static QMutex mutex;
    QMutexLocker locker(&mutex);

    QFile file(QString::fromStdString(Directory::LauncherLogFile()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;

    const char* level = "info";
    switch (type) {
        case QtDebugMsg: level = "debug"; break;
        case QtInfoMsg: level = "info"; break;
        case QtWarningMsg: level = "warn"; break;
        case QtCriticalMsg: level = "critical"; break;
        case QtFatalMsg: level = "fatal"; break;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")) << " [" << level
           << "] " << message << "\n";
}
#endif

struct Settings {
    std::string game_dir;
    std::string selected_version;
    std::string username = "Player";
    int memory_mb = 4096;
};

Settings ParseSettings(const std::string& text) {
    Settings settings;
    if (text.empty()) return settings;

    Json::Value root;
    Json json;
    std::string error;
    if (!json.Parse(text, root, error)) return settings;

    if (root["gameDir"].IsString()) settings.game_dir = root["gameDir"].AsString();
    if (root["selectedVersion"].IsString()) settings.selected_version = root["selectedVersion"].AsString();
    if (root["username"].IsString()) settings.username = root["username"].AsString();
    if (root["memoryMb"].IsNumber()) {
        settings.memory_mb = static_cast<int>(root["memoryMb"].AsInt(4096));
    }
    return settings;
}

// 首次运行：把 src/json 里的默认值抄一份到数据目录
Settings LoadOrCreateSettings() {
    std::string text;
    if (!Directory::ReadDataFile("settings.json", text)) {
        QString json_dir = QCoreApplication::applicationDirPath() + QStringLiteral("/json");
        if (!QDir(json_dir).exists()) json_dir = QStringLiteral(RML_JSON_DIR);
        const std::string defaults =
            Directory::ReadTextFile((json_dir + QStringLiteral("/settings.json")).toStdString());
        if (!defaults.empty()) {
            Directory::WriteDataFile("settings.json", defaults);
            text = defaults;
        }
    }
    return ParseSettings(text);
}

void SaveSettings(const Settings& settings) {
    Json::Value root = Json::Value::Object();
    root.Set("gameDir", Json::Value::String(settings.game_dir));
    root.Set("selectedVersion", Json::Value::String(settings.selected_version));
    root.Set("username", Json::Value::String(settings.username));
    root.Set("memoryMb", Json::Value::Int(settings.memory_mb));
    Directory::WriteDataFile("settings.json", root.Dump());
}

}  // namespace

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("RML"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QDir().mkpath(DataDir());
    QDir().mkpath(LogDir());

#ifdef RML_DEBUG
    qInstallMessageHandler(DebugMessageHandler);
    qInfo() << "启动，数据目录:" << DataDir();
#endif

    Settings settings = LoadOrCreateSettings();
    if (settings.game_dir.empty()) {
        // 默认指向原版启动器的位置：%APPDATA%\.minecraft
        settings.game_dir = Directory::DefaultGameRoot();
    }

    Launch launch(settings.game_dir);
    if (!settings.selected_version.empty()) {
        launch.Versions().Select(settings.selected_version);
    }

    Bridge bridge(launch, QString::fromStdString(settings.username), settings.memory_mb);

    // 服务端模块独立于启动模块；服务器页用自己的桥
    Server server(settings.game_dir);
    ServerBridge server_bridge(server);

#ifdef RML_DEBUG
    qInfo() << "数据目录:" << DataDir();
    qInfo() << "游戏目录:" << bridge.GameDir();
    qInfo() << "版本数:" << bridge.Versions().size() << "Java 数:" << launch.Tools().Javas().size();
    qInfo() << "选中版本:" << bridge.Selected();
#endif

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("serverBridge"), &server_bridge);

#ifdef RML_DEBUG
    engine.rootContext()->setContextProperty(QStringLiteral("rmlDebug"), true);
    engine.rootContext()->setContextProperty(QStringLiteral("rmlShotPath"),
                                             QDir::temp().filePath(QStringLiteral("rml-shot.png")));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { qCritical() << "QML 加载失败"; }, Qt::QueuedConnection);
#endif

    // 部署后优先用 exe 旁边的 qml/，开发时回退到源码目录
    QString qml_dir = QCoreApplication::applicationDirPath() + QStringLiteral("/qml");
    if (!QDir(qml_dir).exists()) qml_dir = QStringLiteral(RML_QML_DIR);
    engine.load(QUrl::fromLocalFile(qml_dir + QStringLiteral("/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;

    const int code = app.exec();

    const QString selected = bridge.Selected();
    if (!selected.isEmpty()) settings.selected_version = selected.toStdString();
    SaveSettings(settings);

    return code;
}
