#include "app/download/VersionSource/Loader/Loader.h"

#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace download {
namespace versionsource {

bool Loader::Fabric(const std::string& mc_version, std::vector<Entry>& out,
                    std::string& error) const {
    const std::string url = "https://meta.fabricmc.net/v2/versions/loader/" + mc_version;

    std::string text;
    if (!cache_.TryGet(url, text, 60 * 60)) {
        if (!tools::HttpGet(url, text, error)) return false;
        cache_.Put(url, text);
    }

    Json::Value root;
    Json json;
    if (!json.Parse(text, root, error)) return false;
    if (!root.IsArray()) {
        error = "fabric meta 不是数组";
        return false;
    }

    out.clear();
    for (size_t i = 0; i < root.Size(); ++i) {
        const Json::Value& item = root.At(i);
        Entry entry;
        entry.version = item["loader"]["version"].AsString();
        entry.stable = item["loader"]["stable"].AsBool(false);
        if (!entry.version.empty()) out.push_back(std::move(entry));
    }
    return true;
}

bool Loader::FabricLatest(const std::string& mc_version, std::string& loader_version,
                          std::string& error) const {
    std::vector<Entry> entries;
    if (!Fabric(mc_version, entries, error)) return false;
    for (const auto& entry : entries) {
        if (entry.stable) {
            loader_version = entry.version;
            return true;
        }
    }
    if (entries.empty()) {
        error = "这个 MC 版本没有可用的 Fabric loader";
        return false;
    }
    loader_version = entries.front().version;
    return true;
}

bool Loader::FabricProfile(const std::string& mc_version, const std::string& loader_version,
                           std::string& json, std::string& error) const {
    const std::string url = "https://meta.fabricmc.net/v2/versions/loader/" + mc_version + "/" +
                            loader_version + "/profile/json";
    if (cache_.TryGet(url, json, 0)) return true;
    if (!tools::HttpGet(url, json, error)) return false;
    cache_.Put(url, json);
    return true;
}

}  // namespace versionsource
}  // namespace download
