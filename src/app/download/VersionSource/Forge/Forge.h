// 版本源：Forge。推荐版本来自 promotions_slim.json；maven 根可配置，
// 非官方源没有 promotions 时退回 maven-metadata 取最新。
#pragma once

#include <string>

#include "app/download/VersionSource/Cache/Cache.h"
#include "app/download/VersionSource/Sources/Sources.h"

namespace download {
namespace versionsource {

class Forge {
public:
    Forge(const Cache& cache, const Sources& sources) : cache_(cache), sources_(sources) {}

    // 某个 MC 版本推荐的 Forge；version 是 maven 上的完整版本号
    bool Latest(const std::string& mc_version, std::string& version, std::string& error) const;

    static std::string InstallerUrl(const std::string& root, const std::string& version);

private:
    bool Promotions(const std::string& root, std::string& json, std::string& error) const;
    bool Versions(const std::string& root, std::string& xml, std::string& error) const;

    const Cache& cache_;
    const Sources& sources_;
};

}  // namespace versionsource
}  // namespace download
