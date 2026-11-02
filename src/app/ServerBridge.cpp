#include "app/ServerBridge.h"

#include <QDesktopServices>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>

#include "app/tools/tools.h"

ServerBridge::ServerBridge(Server& server, QObject* parent)
    : QObject(parent), server_(server) {
    if (!server_.Instances().empty()) selected_ = 0;
    connect(&timer_, &QTimer::timeout, this, &ServerBridge::Tick);
    timer_.start(1000);
}

QVariantList ServerBridge::Instances() const {
    QVariantList out;
    const auto& instances = server_.Instances();
    for (size_t i = 0; i < instances.size(); ++i) {
        QVariantMap map;
        map["name"] = QString::fromStdString(instances[i].name);
        map["status"] = QString::fromStdString(server_.Status(static_cast<int>(i)));
        map["running"] = server_.Running(static_cast<int>(i));
        out.append(map);
    }
    return out;
}

QVariantList ServerBridge::Installations() const {
    QVariantList out;
    for (const auto& installation : server_.Installations()) {
        QVariantMap map;
        map["id"] = QString::fromStdString(installation.id);
        QString label = QString::fromStdString(installation.name);
        if (!installation.loader.empty() || !installation.minecraft.empty()) {
            label += QStringLiteral("  ·  %1 %2")
                         .arg(QString::fromStdString(installation.loader))
                         .arg(QString::fromStdString(installation.minecraft));
        }
        map["label"] = label;
        out.append(map);
    }
    return out;
}

const server::Instance* ServerBridge::Current() const {
    if (selected_ < 0 || static_cast<size_t>(selected_) >= server_.Instances().size()) {
        return nullptr;
    }
    return &server_.Instances()[static_cast<size_t>(selected_)];
}

QString ServerBridge::Name() const {
    const auto* instance = Current();
    return instance ? QString::fromStdString(instance->name) : QString();
}

int ServerBridge::Port() const {
    const auto* instance = Current();
    return instance ? instance->port : 25565;
}

int ServerBridge::MemoryMb() const {
    const auto* instance = Current();
    return instance ? instance->memory_mb : 2048;
}

QString ServerBridge::Gamemode() const {
    const auto* instance = Current();
    return instance ? QString::fromStdString(instance->gamemode) : QStringLiteral("survival");
}

QString ServerBridge::Difficulty() const {
    const auto* instance = Current();
    return instance ? QString::fromStdString(instance->difficulty) : QStringLiteral("easy");
}

int ServerBridge::MaxPlayers() const {
    const auto* instance = Current();
    return instance ? instance->max_players : 20;
}

QString ServerBridge::Status() const {
    if (selected_ < 0 || static_cast<size_t>(selected_) >= server_.Instances().size()) {
        return QStringLiteral("未选择服务器");
    }
    return QString::fromStdString(server_.Status(selected_));
}

bool ServerBridge::Running() const {
    return selected_ >= 0 && static_cast<size_t>(selected_) < server_.Instances().size() &&
           server_.Running(selected_);
}

void ServerBridge::selectAt(int index) {
    if (index < 0 || static_cast<size_t>(index) >= server_.Instances().size()) return;
    if (selected_ == index) return;
    selected_ = index;
    console_ = QString::fromLocal8Bit(server_.Console(index, 64 * 1024).c_str());
    last_console_ = console_;
    message_.clear();
    emit consoleChanged();
    emit changed();
}

void ServerBridge::reloadInstallations() {
    server_.Refresh();
    if (server_.Instances().empty()) {
        selected_ = -1;
    } else if (selected_ < 0) {
        selected_ = 0;
    } else if (static_cast<size_t>(selected_) >= server_.Instances().size()) {
        selected_ = static_cast<int>(server_.Instances().size()) - 1;
    }
    message_ = QStringLiteral("服务端清单已刷新");
    emit changed();
}

void ServerBridge::createServer(const QString& name, int installationIndex, int port,
                                int memoryMb, const QString& gamemode, const QString& difficulty,
                                int maxPlayers) {
    const auto& installations = server_.Installations();
    if (installationIndex < 0 || static_cast<size_t>(installationIndex) >= installations.size()) {
        message_ = QStringLiteral("请选择一个服务端");
        emit changed();
        return;
    }

    Server::CreateOptions options;
    options.name = name.trimmed().toStdString();
    options.installation = installations[static_cast<size_t>(installationIndex)].id;
    options.port = port;
    options.memory_mb = memoryMb;
    options.gamemode = gamemode.toStdString();
    options.difficulty = difficulty.toStdString();
    options.max_players = maxPlayers;

    std::string error;
    if (!server_.Create(options, error)) {
        message_ = QString::fromStdString(error);
        emit changed();
        return;
    }

    selected_ = static_cast<int>(server_.Instances().size()) - 1;
    console_.clear();
    last_console_.clear();
    message_ = QStringLiteral("已创建服务器：%1").arg(name.trimmed());
    emit consoleChanged();
    emit changed();
}

void ServerBridge::startServer() {
    if (selected_ < 0) return;
    std::string error;
    if (!server_.Start(selected_, error)) {
        message_ = QString::fromStdString(error);
    } else {
        message_ = QStringLiteral("正在启动…");
    }
    emit changed();
}

void ServerBridge::stopServer() {
    if (selected_ < 0) return;
    std::string error;
    if (!server_.Stop(selected_, false, error)) {
        message_ = QString::fromStdString(error);
    } else {
        message_ = QStringLiteral("已发送停止命令，等待保存…");
    }
    emit changed();
}

void ServerBridge::forceStopServer() {
    if (selected_ < 0) return;
    std::string error;
    if (!server_.Stop(selected_, true, error)) {
        message_ = QString::fromStdString(error);
    } else {
        message_ = QStringLiteral("已强制停止");
    }
    emit changed();
}

void ServerBridge::sendCommand(const QString& command) {
    if (selected_ < 0 || command.trimmed().isEmpty()) return;
    std::string error;
    if (!server_.Send(selected_, command.trimmed().toStdString(), error)) {
        message_ = QString::fromStdString(error);
        emit changed();
    }
}

void ServerBridge::openFolder() {
    const auto* instance = Current();
    if (instance == nullptr) return;
    const QString path = QString::fromStdString(
        tools::JoinPath(tools::JoinPath(server_.GameDir(), "servers"), instance->dir));
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void ServerBridge::renameServer(const QString& name) {
    if (selected_ < 0) return;
    std::string error;
    if (!server_.Rename(selected_, name.trimmed().toStdString(), error)) {
        message_ = QString::fromStdString(error);
    } else {
        message_ = QStringLiteral("已重命名");
    }
    emit changed();
}

void ServerBridge::removeServer() {
    if (selected_ < 0) return;
    std::string error;
    if (!server_.Remove(selected_, error)) {
        message_ = QString::fromStdString(error);
        emit changed();
        return;
    }
    if (server_.Instances().empty()) {
        selected_ = -1;
    } else {
        selected_ = std::min(selected_, static_cast<int>(server_.Instances().size()) - 1);
    }
    console_.clear();
    last_console_.clear();
    message_ = QStringLiteral("已删除服务器");
    emit consoleChanged();
    emit changed();
}

void ServerBridge::applyParams(int port, int memoryMb, const QString& gamemode,
                               const QString& difficulty, int maxPlayers) {
    if (selected_ < 0) return;
    std::string error;
    if (!server_.Update(selected_, port, memoryMb, gamemode.toStdString(),
                        difficulty.toStdString(), maxPlayers, error)) {
        message_ = QString::fromStdString(error);
    } else {
        message_ = server_.Running(selected_)
                       ? QStringLiteral("参数已保存；端口/玩法重启后生效")
                       : QStringLiteral("参数已保存");
    }
    emit changed();
}

void ServerBridge::Tick() {
    QString statuses;
    const auto& instances = server_.Instances();
    for (size_t i = 0; i < instances.size(); ++i) {
        statuses += QString::fromStdString(instances[i].dir);
        statuses += QLatin1Char('\t');
        statuses += QString::fromStdString(server_.Status(static_cast<int>(i)));
        statuses += QLatin1Char('\n');
    }

    if (statuses != last_statuses_) {
        last_statuses_ = statuses;
        emit changed();
    }

    if (selected_ >= 0) {
        const QString text =
            QString::fromLocal8Bit(server_.Console(selected_, 64 * 1024).c_str());
        if (text != last_console_) {
            last_console_ = text;
            console_ = text;
            emit consoleChanged();
        }
    }
}
