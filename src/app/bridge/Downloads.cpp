#include "app/bridge/Downloads.h"

#include <QMetaObject>

#include <thread>
#include <vector>

#include "app/directory/Directory.h"
#include "app/directory/Flow/Dispatcher.h"
#include "app/server/Installer/Installer.h"
#include "app/tools/tools.h"

namespace {

QString FromStd(const std::string& text) { return QString::fromStdString(text); }

}  // namespace

BridgeDownloads::BridgeDownloads(Launch& launch, NetTask& tasks, QObject* context,
                                 Notify download_changed, Notify changed,
                                 InstalledHandler installed)
    : launch_(launch),
      tasks_(tasks),
      context_(context),
      download_changed_(std::move(download_changed)),
      changed_(std::move(changed)),
      installed_(std::move(installed)),
      content_(launch, tasks, context, state_, download_changed_) {}

BridgeDownloads::~BridgeDownloads() {
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

const QStringList* BridgeDownloads::VersionIdsFor(int kind) const {
    if (kind == 1) return &snapshot_ids_;
    if (kind == 2) return &april_ids_;
    return &release_ids_;
}

void BridgeDownloads::SetStatus(const QString& text) {
    state_.status = text;
    download_changed_();
}

void BridgeDownloads::LoadGameVersions() {
    if (state_.busy) return;
    state_.busy = true;
    state_.failed = false;
    state_.progress = 0.0;
    state_.status = QStringLiteral("正在获取版本清单…");
    download_changed_();

    std::thread worker([this] {
        std::vector<download::versionsource::Mojang::Entry> entries;
        std::string error;
        const bool ok = launch_.Downloader().Sources().Game().List(entries, error);

        QStringList release, snapshot, april;
        QStringList release_ids, snapshot_ids, april_ids;
        if (ok) {
            for (const auto& entry : entries) {
                const QString id = FromStd(entry.id);
                switch (entry.kind) {
                    case download::versionsource::Mojang::Kind::Release:
                        release << id;
                        release_ids << id;
                        break;
                    case download::versionsource::Mojang::Kind::AprilFools:
                        april << id;
                        april_ids << id;
                        break;
                    default:
                        snapshot << id;
                        snapshot_ids << id;
                        break;
                }
            }
        }
        QMetaObject::invokeMethod(
            context_,
            [this, ok, release, snapshot, april, release_ids, snapshot_ids, april_ids, error] {
                release_versions_ = release;
                snapshot_versions_ = snapshot;
                april_versions_ = april;
                release_ids_ = release_ids;
                snapshot_ids_ = snapshot_ids;
                april_ids_ = april_ids;
                state_.status =
                    ok ? QStringLiteral("正式 %1 · 测试 %2 · 愚人节 %3")
                             .arg(release.size())
                             .arg(snapshot.size())
                             .arg(april.size())
                       : QStringLiteral("获取失败：%1").arg(FromStd(error));
                state_.busy = false;
                state_.failed = !ok;
                state_.progress = ok ? 1.0 : 0.0;
                download_changed_();
            },
            Qt::QueuedConnection);
    });
    workers_.push_back(std::move(worker));
}

void BridgeDownloads::LoadLoaderVersions(const QString& mc_version, const QString& loader) {
    if (loader_versions_busy_) return;
    loader_versions_busy_ = true;
    loader_versions_.clear();
    loader_versions_error_.clear();
    download_changed_();

    std::thread worker([this, mc_version, loader] {
        std::vector<std::string> versions;
        std::string error;
        const std::string mc = mc_version.toStdString();
        if (loader == QStringLiteral("Forge")) {
            launch_.Downloader().Sources().Forges().List(mc, versions, error);
        } else if (loader == QStringLiteral("NeoForge")) {
            launch_.Downloader().Sources().NeoForges().List(mc, versions, error);
        } else if (loader == QStringLiteral("Fabric")) {
            std::vector<download::versionsource::Loader::Entry> entries;
            if (launch_.Downloader().Sources().Loaders().Fabric(mc, entries, error)) {
                for (const auto& entry : entries) versions.push_back(entry.version);
            }
        } else {
            error = "不支持的加载器";
        }

        QStringList options;
        for (const auto& version : versions) options << FromStd(version);
        QMetaObject::invokeMethod(
            context_,
            [this, options, error] {
                loader_versions_ = options;
                loader_versions_busy_ = false;
                loader_versions_error_ =
                    options.isEmpty() ? QStringLiteral("没有可用版本：%1").arg(FromStd(error))
                                      : QString();
                download_changed_();
            },
            Qt::QueuedConnection);
    });
    workers_.push_back(std::move(worker));
}

void BridgeDownloads::InstallGame(int kind, int index, const QString& loader,
                                  const QString& loader_version) {
    if (state_.busy) return;

    const QStringList* ids = VersionIdsFor(kind);
    if (index < 0 || index >= ids->size()) return;

    const std::string mc_version = ids->at(index).toStdString();
    const std::string loader_name = loader.toStdString();

    const bool known = loader_name == "原版" || loader_name == "Fabric" ||
                       loader_name == "Forge" || loader_name == "NeoForge";
    if (!known) {
        state_.status = QStringLiteral("不认识的加载器：%1").arg(loader);
        download_changed_();
        return;
    }

    state_.busy = true;
    state_.failed = false;
    state_.progress = 0.0;
    state_.installing_version = FromStd(mc_version);
    launch_.Downloader().Installers().Transfers().ResetCancel();
    state_.status = QStringLiteral("正在安装 %1（%2）…").arg(FromStd(mc_version)).arg(loader);
    download_changed_();

    std::thread worker([this, mc_version, loader_name, loader_version] {
        directory::flow::Context context{launch_.Versions(), launch_.Tools(),
                                        launch_.Downloader().Sources(),
                                        launch_.Downloader().Installers()};

        directory::flow::Request request;
        request.game_dir = launch_.Versions().GameDir();
        request.mc_version = mc_version;
        request.loader = loader_name;
        request.loader_version = loader_version.toStdString();

        auto progress = [this](const std::string& label, int done, int total) {
            if (done != total && done % 4 != 0) return;
            const double fraction = total > 0 ? static_cast<double>(done) / total : 0.0;
            QMetaObject::invokeMethod(
                context_,
                [this, label, fraction] {
                    state_.progress = fraction;
                    state_.status = QStringLiteral("%1  %2%")
                                        .arg(FromStd(label))
                                        .arg(static_cast<int>(fraction * 100.0));
                    download_changed_();
                },
                Qt::QueuedConnection);
        };

        directory::flow::Outcome outcome;
        std::string error;
        const Directory paths(launch_.Versions().GameDir());
        const bool ok = paths.Install(loader_name, context, request, progress, outcome, error);

        QMetaObject::invokeMethod(
            context_,
            [this, ok, outcome, error, loader_name] {
                state_.installing_version.clear();
                state_.busy = false;
                state_.failed = !ok;
                if (ok) state_.progress = 1.0;
                if (ok) {
                    installed_(outcome.version_id);
                    const QString version_tag = outcome.loader_version.empty()
                                                    ? QString()
                                                    : QStringLiteral(" %1").arg(FromStd(outcome.loader_version));
                    state_.status = QStringLiteral("安装完成：%1（%2%3）")
                                        .arg(FromStd(outcome.version_id))
                                        .arg(FromStd(loader_name))
                                        .arg(version_tag);
                } else {
                    state_.status = QStringLiteral("安装失败：%1").arg(FromStd(error));
                }
                download_changed_();
                changed_();
            },
            Qt::QueuedConnection);
    });
    workers_.push_back(std::move(worker));
}

void BridgeDownloads::InstallServer(int kind, int index, const QString& loader) {
    if (state_.busy) return;

    const QStringList* ids = VersionIdsFor(kind);
    if (index < 0 || index >= ids->size()) return;

    const std::string mc_version = ids->at(index).toStdString();
    const std::string loader_name = loader.toStdString();
    if (loader_name != "原版" && loader_name != "Fabric" && loader_name != "Forge") {
        state_.status = QStringLiteral("服务端暂不支持 %1（当前只有原版、Fabric 和 Forge）").arg(loader);
        download_changed_();
        return;
    }

    state_.busy = true;
    state_.failed = false;
    state_.progress = 0.0;
    launch_.Downloader().Installers().Transfers().ResetCancel();
    state_.status = QStringLiteral("正在下载 %1 服务端（%2）…")
                        .arg(FromStd(mc_version))
                        .arg(loader);
    download_changed_();

    std::thread worker([this, mc_version, loader_name] {
        server::Installer installer(launch_.Versions(), launch_.Tools(),
                                    launch_.Downloader().Sources(),
                                    launch_.Downloader().Installers().Transfers());

        auto progress = [this](const std::string& label, int done, int total) {
            if (done != total && done % 16 != 0) return;
            const double fraction = total > 0 ? static_cast<double>(done) / total : 0.0;
            QMetaObject::invokeMethod(
                context_,
                [this, label, fraction] {
                    state_.progress = fraction;
                    state_.status = QStringLiteral("%1  %2%")
                                        .arg(FromStd(label))
                                        .arg(static_cast<int>(fraction * 100.0));
                    download_changed_();
                },
                Qt::QueuedConnection);
        };

        std::string version_id;
        std::string error;
        const bool ok = loader_name == "Fabric"
                            ? installer.Fabric(mc_version, progress, version_id, error)
                            : (loader_name == "Forge"
                                   ? installer.Forge(mc_version, progress, version_id, error)
                                   : installer.Vanilla(mc_version, progress, version_id, error));

        QMetaObject::invokeMethod(
            context_,
            [this, ok, version_id, error] {
                state_.busy = false;
                state_.failed = !ok;
                if (ok) state_.progress = 1.0;
                state_.status = ok ? QStringLiteral("服务端已下载：%1").arg(FromStd(version_id))
                                   : QStringLiteral("服务端下载失败：%1").arg(FromStd(error));
                download_changed_();
            },
            Qt::QueuedConnection);
    });
    workers_.push_back(std::move(worker));
}


