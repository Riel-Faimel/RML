// 原版清单：Mojang 的 version_manifest_v2。
#pragma once

#include <string>
#include <vector>

#include "app/download/VersionSource/Cache/Cache.h"
#include "app/download/VersionSource/Sources/Sources.h"

namespace download {
namespace versionsource {

class Mojang {
public:
    // 版本类别：正式版 / 测试版 / 愚人节版
    enum class Kind { Release, Snapshot, AprilFools };

    struct Entry {
        std::string id;
        std::string type;  // release / snapshot / old_beta / old_alpha
        std::string url;
        Kind kind = Kind::Snapshot;
    };

    Mojang(const Cache& cache, const Sources& sources) : cache_(cache), sources_(sources) {}

    // 可用版本列表；优先用缓存，缓存过期才发请求
    bool List(std::vector<Entry>& out, std::string& error) const;

    // 单个版本的 json 文本（不解析，交给调用方）
    bool Fetch(const std::string& url, std::string& json, std::string& error) const;

    static Kind Classify(const std::string& id, const std::string& type);

private:
    const Cache& cache_;
    const Sources& sources_;
};

}  // namespace versionsource
}  // namespace download
