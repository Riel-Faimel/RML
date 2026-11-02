#include "app/download/VersionSource/Cache/Cache.h"

#include <chrono>
#include <filesystem>

#include "app/tools/tools.h"

namespace download {
namespace versionsource {

Cache::Cache(std::string root) : root_(std::move(root)) {}

std::string Cache::PathFor(const std::string& key) const {
    const std::string hashed = tools::Sha1Hex(key.data(), key.size());
    return tools::JoinPath(root_, hashed + ".txt");
}

bool Cache::TryGet(const std::string& key, std::string& out, int max_age_seconds) const {
    const std::string path = PathFor(key);
    if (!tools::PathExists(path)) return false;

    if (max_age_seconds > 0) {
        std::error_code ec;
        const auto written = std::filesystem::last_write_time(tools::ToPath(path), ec);
        if (ec) return false;
        const auto age = std::filesystem::file_time_type::clock::now() - written;
        if (age > std::chrono::seconds(max_age_seconds)) return false;
    }

    out = tools::ReadTextFile(path);
    return !out.empty();
}

void Cache::Put(const std::string& key, const std::string& value) const {
    std::string error;
    tools::EnsureDir(root_, error);
    tools::WriteTextFile(PathFor(key), value);
}

}  // namespace versionsource
}  // namespace download
