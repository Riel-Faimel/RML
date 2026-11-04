// 应用层标签：加载器识别与软件源显示名。
#pragma once

#include <QString>

#include <string>

class BridgeLabels {
public:
    // 从主类判断加载器，够用就行，不解析 mod 目录
    static QString LoaderFromMainClass(const std::string& main_class);

    // 版本 id / rml.json 的 loader → 内容源使用的加载器 id（forge/fabric/neoforge/quilt）
    static std::string ContentLoaderId(const std::string& loader, const std::string& id);

    static QString SourceKindLabel(const std::string& kind);
};
