// 加载器清单：某个 MC 版本能配哪些 loader 版本。
#pragma once

#include <string>
#include <vector>

#include "app/download/VersionSource/Cache/Cache.h"
#include "app/download/VersionSource/Sources/Sources.h"

namespace download {
namespace versionsource {

class Loader {
public:
    struct Entry {
        std::string version;
        bool stable = false;
    };

    Loader(const Cache& cache, const Sources& sources) : cache_(cache), sources_(sources) {}

    bool Fabric(const std::string& mc_version, std::vector<Entry>& out, std::string& error) const;

    // 某 MC 版本最新的稳定 Fabric loader
    bool FabricLatest(const std::string& mc_version, std::string& loader_version,
                      std::string& error) const;

    // Fabric 直接提供一份可用的版本 json（含 inheritsFrom / libraries / mainClass）
    bool FabricProfile(const std::string& mc_version, const std::string& loader_version,
                       std::string& json, std::string& error) const;

    // Fabric 服务端的版本描述（mainClass + libraries），用来拼服务端启动命令
    bool FabricServerProfile(const std::string& mc_version, const std::string& loader_version,
                             std::string& json, std::string& error) const;

private:
    const Cache& cache_;
    const Sources& sources_;
};

}  // namespace versionsource
}  // namespace download
