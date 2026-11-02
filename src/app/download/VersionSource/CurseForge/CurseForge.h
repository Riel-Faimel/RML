// 内容源：CurseForge。需要 API key，放在 %APPDATA%\RML\curseforge.key 里。
#pragma once

#include <string>

#include "app/download/VersionSource/Cache/Cache.h"
#include "include/ContentSource.h"

namespace download {
namespace versionsource {

class CurseForge : public ContentSource {
public:
    explicit CurseForge(const Cache& cache) : cache_(cache) {}

    std::string Id() const override { return "curseforge"; }
    std::string DisplayName() const override { return "CurseForge"; }
    bool Ready(std::string& reason) const override;

    bool Search(const std::string& query, const std::string& loader, const std::string& mc_version,
                std::vector<Project>& out, std::string& error) override;
    bool Versions(const std::string& project_id, const std::string& loader,
                  const std::string& mc_version, std::vector<Version>& out,
                  std::string& error) override;

private:
    static std::string ApiKey();

    const Cache& cache_;
};

}  // namespace versionsource
}  // namespace download
