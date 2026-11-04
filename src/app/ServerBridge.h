// 应用层：把服务端模块暴露给服务器页。与 Bridge 分开，避免主桥无限膨胀。
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include "app/server/Server.h"

class ServerBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList instances READ Instances NOTIFY changed)
    Q_PROPERTY(QVariantList installations READ Installations NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ SelectedIndex NOTIFY changed)
    Q_PROPERTY(QString name READ Name NOTIFY changed)
    Q_PROPERTY(int port READ Port NOTIFY changed)
    Q_PROPERTY(int memoryMb READ MemoryMb NOTIFY changed)
    Q_PROPERTY(QString gamemode READ Gamemode NOTIFY changed)
    Q_PROPERTY(QString difficulty READ Difficulty NOTIFY changed)
    Q_PROPERTY(int maxPlayers READ MaxPlayers NOTIFY changed)
    Q_PROPERTY(QString status READ Status NOTIFY changed)
    Q_PROPERTY(QString consoleText READ ConsoleText NOTIFY consoleChanged)
    Q_PROPERTY(bool running READ Running NOTIFY changed)
    Q_PROPERTY(QString message READ Message NOTIFY changed)

public:
    explicit ServerBridge(Server& server, QObject* parent = nullptr);

    QVariantList Instances() const;
    QVariantList Installations() const;
    int SelectedIndex() const { return selected_; }
    QString Name() const;
    int Port() const;
    int MemoryMb() const;
    QString Gamemode() const;
    QString Difficulty() const;
    int MaxPlayers() const;
    QString Status() const;
    QString ConsoleText() const { return console_; }
    bool Running() const;
    QString Message() const { return message_; }

public slots:
    void reloadInstallations();
    void selectAt(int index);
    void createServer(const QString& name, int installationIndex, int port, int memoryMb,
                      const QString& gamemode, const QString& difficulty, int maxPlayers,
                      const QString& seed);
    void startServer();
    void stopServer();
    void forceStopServer();
    void sendCommand(const QString& command);
    void openFolder();
    void renameServer(const QString& name);
    void removeServerAt(int index);
    void removeInstallationAt(int index);
    void applyParams(int port, int memoryMb, const QString& gamemode, const QString& difficulty,
                     int maxPlayers);

    // server.properties 详细编辑
    QVariantList readServerProperties();
    bool saveServerProperties(const QVariantList& entries);

signals:
    void changed();
    void consoleChanged();

private slots:
    void Tick();

private:
    const server::Instance* Current() const;

    Server& server_;
    QTimer timer_;
    int selected_ = -1;
    QString console_;
    QString message_;
    QString last_statuses_;
    QString last_console_;
};
