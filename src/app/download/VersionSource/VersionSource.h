// 版本源：所有“有什么”的来源都从这里拿，安装器只读它、不写它。
#pragma once

#include <string>
#include <vector>

#include "app/download/VersionSource/Cache/Cache.h"
#include "app/download/VersionSource/CurseForge/CurseForge.h"
#include "app/download/VersionSource/Forge/Forge.h"
#include "app/download/VersionSource/Loader/Loader.h"
#include "app/download/VersionSource/Mojang/Mojang.h"
#include "app/download/VersionSource/Modrinth/Modrinth.h"
#include "app/download/VersionSource/NeoForge/NeoForge.h"
#include "app/download/VersionSource/Sources/Sources.h"

namespace download {

class VersionSource {
public:
    VersionSource();

    // 官方版本（Mojang）
    versionsource::Mojang& Game() { return mojang_; }
    versionsource::Loader& Loaders() { return loader_; }

    // 加载器推荐版本（Forge / NeoForge）
    versionsource::Forge& Forges() { return forge_; }
    versionsource::NeoForge& NeoForges() { return neoforge_; }

    // 软件源配置（设置页可改）
    versionsource::Sources& Origins() { return sources_; }

    // 内容源（Modrinth / CurseForge）
    std::vector<ContentSource*> Sources();

private:
    versionsource::Sources sources_;
    versionsource::Cache cache_;
    versionsource::Mojang mojang_;
    versionsource::Loader loader_;
    versionsource::Forge forge_;
    versionsource::NeoForge neoforge_;
    versionsource::Modrinth modrinth_;
    versionsource::CurseForge curseforge_;
};

}  // namespace download
