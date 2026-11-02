#include "app/download/VersionSource/Mojang/Mojang.h"

#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace download {
namespace versionsource {

namespace {

constexpr const char* kMojangUrl =
    "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
constexpr int kMojangMaxAgeSeconds = 6 * 60 * 60;

}  // namespace

bool Mojang::List(std::vector<Entry>& out, std::string& error) const {
    std::string text;
    if (!cache_.TryGet(kMojangUrl, text, kMojangMaxAgeSeconds)) {
        if (!tools::HttpGet(kMojangUrl, text, error)) return false;
        cache_.Put(kMojangUrl, text);
    }

    Json::Value root;
    Json json;
    if (!json.Parse(text, root, error)) return false;

    const Json::Value& versions = root["versions"];
    if (!versions.IsArray()) {
        error = "manifest.versions 不是数组";
        return false;
    }

    out.clear();
    out.reserve(versions.Size());
    for (size_t i = 0; i < versions.Size(); ++i) {
        const Json::Value& item = versions.At(i);
        Entry entry;
        entry.id = item["id"].AsString();
        entry.type = item["type"].AsString();
        entry.url = item["url"].AsString();
        if (!entry.id.empty() && !entry.url.empty()) out.push_back(std::move(entry));
    }
    return true;
}

bool Mojang::Fetch(const std::string& url, std::string& json, std::string& error) const {
    if (cache_.TryGet(url, json, 0)) return true;
    if (!tools::HttpGet(url, json, error)) return false;
    cache_.Put(url, json);
    return true;
}

}  // namespace versionsource
}  // namespace download
