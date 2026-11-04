// 版本源：NeoForge。maven 根可配置；1.20.1 在 net/neoforged/forge，其余在 net/neoforged/neoforge。
#pragma once

#include <string>
#include <vector>

#include "app/download/VersionSource/Cache/Cache.h"
#include "app/download/VersionSource/Sources/Sources.h"

namespace download {
namespace versionsource {

class NeoForge {
public:
    NeoForge(const Cache& cache, const Sources& sources) : cache_(cache), sources_(sources) {}

    // 某个 MC 版本最新的 NeoForge；version 是 maven 上的完整版本号
    bool Latest(const std::string& mc_version, std::string& version, std::string& error) const;

    // 某个 MC 版本的全部 NeoForge 版本，新的在前
    bool List(const std::string& mc_version, std::vector<std::string>& out,
              std::string& error) const;

    static std::string InstallerUrl(const std::string& root, const std::string& version);

private:
    bool Metadata(const std::string& url, std::string& xml, std::string& error) const;

    const Cache& cache_;
    const Sources& sources_;
};

}  // namespace versionsource
}  // namespace download
