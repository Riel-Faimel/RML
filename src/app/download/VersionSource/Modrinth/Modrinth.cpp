#include "app/network/Network.h"
#include "app/download/VersionSource/Modrinth/Modrinth.h"

#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace download {
namespace versionsource {

namespace {

constexpr int kMaxAgeSeconds = 10 * 60;

std::string UrlEncode(const std::string& text) {
    std::string out;
    static const char* hex = "0123456789ABCDEF";
    for (unsigned char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

std::string Quote(const std::string& text) { return "%22" + UrlEncode(text) + "%22"; }

}  // namespace

bool Modrinth::Search(const std::string& query, const std::string& kind,
                      const std::string& loader, const std::string& mc_version,
                      std::vector<Project>& out, std::string& error) {
    std::string url = "https://api.modrinth.com/v2/search?limit=20&query=" + UrlEncode(query);
    if (!kind.empty() || !loader.empty() || !mc_version.empty()) {
        std::string facets = "&facets=%5B";
        bool first = true;
        if (!kind.empty()) {
            facets += "%5B%22project_type%3A" + UrlEncode(kind) + "%22%5D";
            first = false;
        }
        if (!loader.empty() && kind == "mod") {
            if (!first) facets += ",";
            facets += "%5B%22categories%3A" + UrlEncode(loader) + "%22%5D";
            first = false;
        }
        if (!mc_version.empty()) {
            if (!first) facets += ",";
            facets += "%5B%22versions%3A" + UrlEncode(mc_version) + "%22%5D";
        }
        facets += "%5D";
        url += facets;
    }

    std::string text;
    const std::string cache_key = "modrinth:search:" + url;
    if (!cache_.TryGet(cache_key, text, kMaxAgeSeconds)) {
        if (!Network::http(HttpCMD::GET, url, text, error)) return false;
        cache_.Put(cache_key, text);
    }

    Json::Value root;
    Json json;
    if (!json.Parse(text, root, error)) return false;

    const Json::Value& hits = root["hits"];
    if (!hits.IsArray()) {
        error = "search.hits 不是数组";
        return false;
    }

    out.clear();
    for (size_t i = 0; i < hits.Size(); ++i) {
        const Json::Value& item = hits.At(i);
        Project project;
        project.id = item["project_id"].AsString();
        project.slug = item["slug"].AsString();
        project.title = item["title"].AsString();
        project.icon_url = item["icon_url"].AsString();
        project.description = item["description"].AsString();
        if (!project.id.empty()) out.push_back(std::move(project));
    }
    return true;
}

bool Modrinth::Versions(const std::string& project_id, const std::string& loader,
                        const std::string& mc_version, std::vector<Version>& out,
                        std::string& error) {
    std::string url = "https://api.modrinth.com/v2/project/" + UrlEncode(project_id) + "/version?";
    if (!loader.empty()) url += "loaders=%5B" + Quote(loader) + "%5D&";
    if (!mc_version.empty()) url += "game_versions=%5B" + Quote(mc_version) + "%5D&";
    if (url.back() == '&') url.pop_back();

    std::string text;
    const std::string cache_key = "modrinth:versions:" + url;
    if (!cache_.TryGet(cache_key, text, kMaxAgeSeconds)) {
        if (!Network::http(HttpCMD::GET, url, text, error)) return false;
        cache_.Put(cache_key, text);
    }

    Json::Value root;
    Json json;
    if (!json.Parse(text, root, error)) return false;
    if (!root.IsArray()) {
        error = "版本列表不是数组";
        return false;
    }

    out.clear();
    for (size_t i = 0; i < root.Size(); ++i) {
        const Json::Value& item = root.At(i);
        Version version;
        version.id = item["id"].AsString();
        version.name = item["name"].AsString();
        version.version_number = item["version_number"].AsString();

        const Json::Value& game_versions = item["game_versions"];
        for (size_t k = 0; k < game_versions.Size(); ++k) {
            version.game_versions.push_back(game_versions.At(k).AsString());
        }
        const Json::Value& loaders = item["loaders"];
        for (size_t k = 0; k < loaders.Size(); ++k) {
            version.loaders.push_back(loaders.At(k).AsString());
        }
        const Json::Value& files = item["files"];
        for (size_t k = 0; k < files.Size(); ++k) {
            const Json::Value& file = files.At(k);
            File parsed;
            parsed.url = file["url"].AsString();
            parsed.filename = file["filename"].AsString();
            parsed.sha1 = file["hashes"]["sha1"].AsString();
            parsed.size = file["size"].AsInt(0);
            if (!parsed.url.empty()) version.files.push_back(std::move(parsed));
        }
        if (!version.id.empty()) out.push_back(std::move(version));
    }
    return true;
}

}  // namespace versionsource
}  // namespace download
