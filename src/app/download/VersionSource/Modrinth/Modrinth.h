// 内容源：Modrinth。开放 API，不需要 key。
#pragma once

#include <string>

#include "app/download/VersionSource/Cache/Cache.h"
#include "include/ContentSource.h"

namespace download {
namespace versionsource {

class Modrinth : public ContentSource {
public:
    explicit Modrinth(const Cache& cache) : cache_(cache) {}

    std::string DisplayName() const override { return "Modrinth"; }
    bool Ready(std::string& reason) const override {
        (void)reason;
        return true;
    }

    bool Search(const std::string& query, const std::string& kind, const std::string& loader,
                const std::string& mc_version, std::vector<Project>& out,
                std::string& error) override;
    bool Versions(const std::string& project_id, const std::string& loader,
                  const std::string& mc_version, std::vector<Version>& out,
                  std::string& error) override;

private:
    const Cache& cache_;
};

}  // namespace versionsource
}  // namespace download
