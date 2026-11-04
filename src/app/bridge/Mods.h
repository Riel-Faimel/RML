// Mod 仓库与版本 mods 目录：列表、安装、移除。
#pragma once

#include <QString>
#include <QVariantList>

#include <string>

#include "app/directory/Directory.h"

class BridgeMods {
public:
    // 版本独立 Mod 仓库（<gameDir>/mods）
    QVariantList Repository(const std::string& game_dir, const Directory::VersionEntry& entry);

    // 版本自己的 mods 目录（<gameDir>/versions/<id>/mods）
    QVariantList Installed(const std::string& game_dir, const Directory::VersionEntry& entry);

    // index 对应 Repository() 返回的列表
    bool Install(const std::string& game_dir, const Directory::VersionEntry& entry, int index,
                 QString& message);

    // index 对应 Installed() 返回的列表
    bool Remove(const std::string& game_dir, const Directory::VersionEntry& entry, int index,
                QString& message);

private:
    QVariantList repository_items_;
    QVariantList installed_items_;
};
