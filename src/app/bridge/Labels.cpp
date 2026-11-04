#include "app/bridge/Labels.h"

#include <cctype>

namespace {

QString FromStd(const std::string& text) { return QString::fromStdString(text); }

}  // namespace

QString BridgeLabels::LoaderFromMainClass(const std::string& main_class) {
    if (main_class.find("fabric") != std::string::npos) return QStringLiteral("Fabric");
    if (main_class.find("quilt") != std::string::npos) return QStringLiteral("Quilt");
    if (main_class.find("neoforge") != std::string::npos) return QStringLiteral("NeoForge");
    if (main_class.find("forge") != std::string::npos) return QStringLiteral("Forge");
    // 1.16~1.20 的 Forge/NeoForge 都从 modlauncher 启动；没有 rml.json 的外部版本靠这个兜底
    if (main_class.find("modlauncher") != std::string::npos) return QStringLiteral("Forge");
    return QStringLiteral("原版");
}

std::string BridgeLabels::ContentLoaderId(const std::string& loader, const std::string& id) {
    std::string value = loader;
    if (value.empty()) {
        if (id.find("-neoforge-") != std::string::npos) return "neoforge";
        if (id.find("-forge-") != std::string::npos) return "forge";
        if (id.find("-fabric-") != std::string::npos) return "fabric";
        if (id.find("-quilt-") != std::string::npos) return "quilt";
        return {};
    }
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (value == "forge") return "forge";
    if (value == "neoforge") return "neoforge";
    if (value == "fabric") return "fabric";
    if (value == "quilt") return "quilt";
    return {};
}

QString BridgeLabels::SourceKindLabel(const std::string& kind) {
    if (kind == "mojang") return QStringLiteral("Mojang 版本清单");
    if (kind == "assets") return QStringLiteral("Mojang 资源");
    if (kind == "forge") return QStringLiteral("Forge maven");
    if (kind == "neoforge") return QStringLiteral("NeoForge maven");
    if (kind == "fabric") return QStringLiteral("Fabric meta");
    return FromStd(kind);
}


