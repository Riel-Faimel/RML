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

}  // namespace versionsource
}  // namespace download
