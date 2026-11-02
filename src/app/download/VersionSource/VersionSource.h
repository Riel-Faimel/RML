// 版本源：所有“有什么”的来源都从这里拿，安装器只读它、不写它。
#pragma once

#include <string>
#include <vector>

#include "app/download/VersionSource/Cache/Cache.h"
#include "app/download/VersionSource/CurseForge/CurseForge.h"
#include "app/download/VersionSource/Loader/Loader.h"
#include "app/download/VersionSource/Mojang/Mojang.h"
#include "app/download/VersionSource/Modrinth/Modrinth.h"

namespace download {

class VersionSource {
public:
    VersionSource();

    // 官方版本（Mojang）
    versionsource::Mojang& Game() { return mojang_; }
    versionsource::Loader& Loaders() { return loader_; }

    // 内容源（Modrinth / CurseForge）
    std::vector<ContentSource*> Sources();
    ContentSource* Source(const std::string& id);

private:
    versionsource::Cache cache_;
    versionsource::Mojang mojang_;
    versionsource::Loader loader_;
    versionsource::Modrinth modrinth_;
    versionsource::CurseForge curseforge_;
};

}  // namespace download
