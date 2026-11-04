#include "app/network/Network.h"
#include "app/download/VersionSource/Mojang/Mojang.h"

#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace download {
namespace versionsource {

namespace {

constexpr int kMojangMaxAgeSeconds = 6 * 60 * 60;

// Mojang 清单只标 release / snapshot，愚人节版是“伪装成快照”的，
// 官方没有类型字段，只能靠已知 id 认。
bool IsAprilFools(const std::string& id) {
    static const char* kIds[] = {
        "2.0",                   // 2013
        "15w14a",                // 2015
        "1.rv-pre1",             // 2016
        "3d shareware v1.34",    // 2019
        "20w14infinite",         // 2020
        "22w13oneblockatatime",  // 2022
        "23w13a_or_b",           // 2023
        "24w14potato",           // 2024
        "25w14craftmine",        // 2025
    };
    const std::string lowered = tools::ToLower(id);
    for (const char* known : kIds) {
        if (lowered == known) return true;
    }
    return false;
}

}  // namespace

Mojang::Kind Mojang::Classify(const std::string& id, const std::string& type) {
    if (IsAprilFools(id)) return Kind::AprilFools;
    if (type == "release") return Kind::Release;
    return Kind::Snapshot;
}

bool Mojang::List(std::vector<Entry>& out, std::string& error) const {
    const std::string url = sources_.ManifestUrl();

    std::string text;
    if (!cache_.TryGet(url, text, kMojangMaxAgeSeconds)) {
        if (!Network::http(HttpCMD::GET, url, text, error)) return false;
        cache_.Put(url, text);
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
        entry.kind = Classify(entry.id, entry.type);
        if (!entry.id.empty() && !entry.url.empty()) out.push_back(std::move(entry));
    }
    return true;
}

bool Mojang::Fetch(const std::string& url, std::string& json, std::string& error) const {
    if (cache_.TryGet(url, json, 0)) return true;
    if (!Network::http(HttpCMD::GET, url, json, error)) return false;
    cache_.Put(url, json);
    return true;
}

}  // namespace versionsource
}  // namespace download
