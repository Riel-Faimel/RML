// 加载器清单：某个 MC 版本能配哪些 loader 版本。
#pragma once

#include <string>
#include <vector>

#include "app/download/VersionSource/Cache/Cache.h"

namespace download {
namespace versionsource {

class Loader {
public:
    struct Entry {
        std::string version;
        bool stable = false;
    };

    explicit Loader(const Cache& cache) : cache_(cache) {}

    bool Fabric(const std::string& mc_version, std::vector<Entry>& out, std::string& error) const;

private:
    const Cache& cache_;
};

}  // namespace versionsource
}  // namespace download
