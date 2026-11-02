// 版本源的缓存：远端清单拿一次就存下来，断网时还能用上一次的结果。
#pragma once

#include <string>

namespace download {
namespace versionsource {

class Cache {
public:
    explicit Cache(std::string root);

    // 命中且未过期返回 true
    bool TryGet(const std::string& key, std::string& out, int max_age_seconds) const;
    void Put(const std::string& key, const std::string& value) const;

private:
    std::string PathFor(const std::string& key) const;

    std::string root_;
};

}  // namespace versionsource
}  // namespace download
