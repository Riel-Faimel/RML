#include "app/bridge/Versions.h"

#include <QMetaObject>
#include <QMetaType>
#include <QString>

#include <filesystem>
#include <thread>
#include <utility>

#include "app/bridge/Labels.h"
#include "app/tools/tools.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

QString FromStd(const std::string& text) { return QString::fromStdString(text); }

}  // namespace

BridgeVersions::BridgeVersions(Launch& launch, QObject* context)
    : launch_(launch), context_(context) {
    Refresh();

    if (!names_.isEmpty()) {
        // 尊重 settings.json 里的选择；只有没有选择时才退到第一个
        const std::string saved = launch_.Versions().Selected();
        if (!saved.empty() && ids_.contains(FromStd(saved))) {
            selected_ = FromStd(saved);
            selected_index_ = ids_.indexOf(selected_);
        } else {
            selected_index_ = 0;
            selected_ = names_.first();
            launch_.Versions().Select(ids_.first().toStdString());
        }
        UpdateDetails();
    }

    RefreshJavas();
    if (javas_.isEmpty()) java_info_ = QStringLiteral("未发现 Java");
}

QString BridgeVersions::SelectedName() const {
    if (selected_index_ >= 0 && selected_index_ < names_.size()) {
        return names_.at(selected_index_);
    }
    return selected_;
}

void BridgeVersions::Refresh() {
    launch_.Versions().Refresh();
    names_.clear();
    ids_.clear();
    for (const auto& entry : launch_.Versions().Entries()) {
        // 服务端安装不参与启动页
        if (entry.is_server) continue;
        // 安装中途失败的托管版本不进入启动页，避免点进去直接退出码 1
        if (entry.managed && !entry.complete) continue;
        names_ << FromStd(entry.name);
        ids_ << FromStd(entry.id);
    }

    selected_ = FromStd(launch_.Versions().Selected());
    selected_index_ = ids_.indexOf(selected_);
}

void BridgeVersions::UpdateDetails() {
    launch::Version::Detail detail;
    std::string error;
    if (!launch_.Versions().LoadDetail(detail, error)) {
        details_ = QStringLiteral("解析失败：%1").arg(FromStd(error));
        return;
    }

    const auto& java = launch_.Tools().Pick(detail.java_major);
    const QString java_line =
        java ? QStringLiteral("使用时 %1（Java %2）").arg(FromStd(java->path)).arg(java->major)
             : QStringLiteral("没有可用 Java");

    loader_ = BridgeLabels::LoaderFromMainClass(detail.main_class);

    // 版本来源：RML 装的有 rml.json 标记，其余是外部放进来的
    QString origin = QStringLiteral("外部版本");
    for (const auto& entry : launch_.Versions().Entries()) {
        if (entry.id != selected_.toStdString()) continue;
        if (!entry.loader.empty()) loader_ = FromStd(entry.loader);
        if (entry.managed) {
            origin = entry.loader.empty() ? QStringLiteral("RML 安装")
                                          : QStringLiteral("RML 安装 · %1").arg(FromStd(entry.loader));
            if (!entry.complete) origin += QStringLiteral("（未完成）");
        }
        break;
    }

    details_ = QStringLiteral("%1\n需要 Java %2\n库文件 %3 个\n客户端 jar %4")
                   .arg(origin)
                   .arg(detail.java_major)
                   .arg(detail.libraries.size())
                   .arg(detail.client_jar.empty() ? QStringLiteral("缺失") : QStringLiteral("就绪"));
    java_info_ = java_line;
}

void BridgeVersions::RefreshJavas() {
    javas_.clear();
    java_options_.clear();
    java_paths_.clear();

    java_options_ << QStringLiteral("自动（推荐）");

    for (const auto& java : launch_.Tools().Javas()) {
        const bool managed = java.source == "RML";
        const QString tag = managed ? QStringLiteral("RML 安装") : QStringLiteral("系统");

        // 设置页：来源标记放尾部
        javas_ << QStringLiteral("Java %1  ·  %2  ·  %3")
                      .arg(java.major)
                      .arg(FromStd(java.path))
                      .arg(tag);

        // 启动页下拉：短一点
        java_options_ << QStringLiteral("Java %1  ·  %2").arg(java.major).arg(tag);
        java_paths_ << FromStd(java.path);
    }
}

bool BridgeVersions::SelectAt(int index) {
    const std::string id = VersionIdAt(index);
    if (id.empty() || !launch_.Versions().Select(id)) return false;
    selected_ = FromStd(id);
    selected_index_ = index;
    UpdateDetails();
    return true;
}

void BridgeVersions::SelectInstalled(const std::string& id) {
    Refresh();
    if (!launch_.Versions().Select(id)) return;
    selected_ = FromStd(id);
    selected_index_ = ids_.indexOf(selected_);
    UpdateDetails();
}

void BridgeVersions::OpenFolderAt(int index) {
    const std::string id = VersionIdAt(index);
    if (id.empty()) return;
    const std::string dir = launch_.Versions().VersionDir(id);
#ifdef _WIN32
    const std::wstring wide(dir.begin(), dir.end());
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    (void)dir;
#endif
}

bool BridgeVersions::DeleteAt(int index, std::string& error) {
    const std::string id = VersionIdAt(index);
    if (id.empty()) {
        error = "版本不存在";
        return false;
    }

    const std::string dir = launch_.Versions().VersionDir(id);
    std::error_code ec;
    std::filesystem::remove_all(tools::ToPath(dir), ec);
    if (ec) {
        error = "删除版本目录失败: " + dir + "（" + ec.message() + "）";
        return false;
    }

    // 只删入口；核心保留，由设置页的 GC 显式清理。
    launch_.Versions().Refresh();
    Refresh();
    if (names_.isEmpty()) {
        selected_.clear();
        selected_index_ = -1;
        details_.clear();
    } else {
        SelectAt(qMin(index, static_cast<int>(names_.size()) - 1));
    }
    return true;
}

void BridgeVersions::RenameAt(int index, const QString& name) {
    const std::string id = VersionIdAt(index);
    if (id.empty() || name.trimmed().isEmpty()) return;
    launch_.Versions().Rename(id, name.trimmed().toStdString());
    Refresh();
}

void BridgeVersions::SelectJava(int index) {
    if (index <= 0 || index > java_paths_.size()) {
        java_path_override_.clear();
    } else {
        java_path_override_ = java_paths_.at(index - 1);
    }
    UpdateDetails();
}

bool BridgeVersions::CanUninstallJava(int index) const {
    if (index < 0 || index >= java_paths_.size()) return false;
    const auto& javas = launch_.Tools().Javas();
    if (index >= static_cast<int>(javas.size())) return false;
    return javas[index].source == "RML";
}

bool BridgeVersions::UninstallJava(int index, std::string& error) {
    if (!CanUninstallJava(index)) {
        error = "只能卸载 RML 安装的 Java";
        return false;
    }
    const std::string path = launch_.Tools().Javas()[index].path;
    if (!launch_.Tools().Uninstall(path, error)) return false;

    if (java_path_override_ == FromStd(path)) java_path_override_.clear();
    RefreshJavas();
    UpdateDetails();
    return true;
}

void BridgeVersions::InstallJava(int major, std::function<void(bool, const QString&)> done) {
    std::thread([this, major, done = std::move(done)] {
        std::string error;
        const bool ok = launch_.Tools().Install(major, error);
        const QString message = QString::fromStdString(error);
        QMetaObject::invokeMethod(
            context_,
            [done = std::move(done), ok, message] { done(ok, message); },
            Qt::QueuedConnection);
    }).detach();
}

std::string BridgeVersions::VersionIdAt(int index) const {
    if (index < 0 || index >= ids_.size()) return {};
    return ids_.at(index).toStdString();
}

const Directory::VersionEntry* BridgeVersions::SelectedEntry() const {
    for (const auto& entry : launch_.Versions().Entries()) {
        if (entry.id == selected_.toStdString()) return &entry;
    }
    return nullptr;
}

std::string BridgeVersions::SelectedMinecraftVersion() const {
    const Directory::VersionEntry* entry = SelectedEntry();
    return entry == nullptr ? std::string() : entry->minecraft_version;
}

std::string BridgeVersions::SelectedContentLoader() const {
    const Directory::VersionEntry* entry = SelectedEntry();
    if (entry == nullptr) return {};
    return BridgeLabels::ContentLoaderId(entry->loader, entry->id);
}


