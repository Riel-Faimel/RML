#include "app/download/VersionSource/CurseForge/CurseForge.h"

#include "app/directory/Directory.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace download {
namespace versionsource {

namespace {

constexpr int kMaxAgeSeconds = 10 * 60;
constexpr const char* kApiBase = "https://api.curseforge.com/v1";
constexpr int kGameMinecraft = 432;
constexpr int kClassMods = 6;

std::string UrlEncodeShim(const std::string& text) {
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

std::string LoaderType(const std::string& loader) {
    if (loader == "forge") return "1";
    if (loader == "fabric") return "4";
    if (loader == "quilt") return "5";
    if (loader == "neoforge") return "6";
    return {};
}

std::vector<std::string> AuthHeaders(const std::string& key) {
    return {"x-api-key: " + key, "Accept: application/json"};
}

}  // namespace

std::string CurseForge::ApiKey() {
    const std::string path = Directory::DataFile("curseforge.key");
    std::string text = tools::ReadTextFile(path);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
        text.pop_back();
    }
    return text;
}

bool CurseForge::Ready(std::string& reason) const {
    if (ApiKey().empty()) {
        reason = "需要 %APPDATA%\\RML\\curseforge.key";
        return false;
    }
    return true;
}

bool CurseForge::Search(const std::string& query, const std::string& loader,
                        const std::string& mc_version, std::vector<Project>& out,
                        std::string& error) {
    std::string reason;
    if (!Ready(reason)) {
        error = reason;
        return false;
    }

    std::string url = std::string(kApiBase) + "/mods/search?gameId=" + std::to_string(kGameMinecraft) +
                      "&classId=" + std::to_string(kClassMods) + "&pageSize=20&searchFilter=" +
                      UrlEncodeShim(query);
    if (!mc_version.empty()) url += "&gameVersion=" + UrlEncodeShim(mc_version);
    const std::string loader_type = LoaderType(loader);
    if (!loader_type.empty()) url += "&modLoaderType=" + loader_type;

    std::string text;
    const std::string cache_key = "curseforge:search:" + url;
    if (!cache_.TryGet(cache_key, text, kMaxAgeSeconds)) {
        if (!tools::HttpGet(url, AuthHeaders(ApiKey()), text, error)) return false;
        cache_.Put(cache_key, text);
    }

    Json::Value root;
    Json json;
    if (!json.Parse(text, root, error)) return false;

    const Json::Value& data = root["data"];
    if (!data.IsArray()) {
        error = "curseforge 返回里没有 data";
        return false;
    }

    out.clear();
    for (size_t i = 0; i < data.Size(); ++i) {
        const Json::Value& item = data.At(i);
        Project project;
        project.id = std::to_string(item["id"].AsInt(0));
        project.slug = item["slug"].AsString();
        project.title = item["name"].AsString();
        project.description = item["summary"].AsString();
        project.downloads = item["downloadCount"].AsInt(0);
        if (project.id != "0") out.push_back(std::move(project));
    }
    (void)mc_version;
    return true;
}

bool CurseForge::Versions(const std::string& project_id, const std::string& loader,
                          const std::string& mc_version, std::vector<Version>& out,
                          std::string& error) {
    std::string reason;
    if (!Ready(reason)) {
        error = reason;
        return false;
    }

    std::string url =
        std::string(kApiBase) + "/mods/" + project_id + "/files?pageSize=20";
    if (!mc_version.empty()) url += "&gameVersion=" + UrlEncodeShim(mc_version);
    const std::string loader_type = LoaderType(loader);
    if (!loader_type.empty()) url += "&modLoaderType=" + loader_type;

    std::string text;
    const std::string cache_key = "curseforge:files:" + url;
    if (!cache_.TryGet(cache_key, text, kMaxAgeSeconds)) {
        if (!tools::HttpGet(url, AuthHeaders(ApiKey()), text, error)) return false;
        cache_.Put(cache_key, text);
    }

    Json::Value root;
    Json json;
    if (!json.Parse(text, root, error)) return false;

    const Json::Value& data = root["data"];
    if (!data.IsArray()) {
        error = "curseforge 返回里没有 data";
        return false;
    }

    out.clear();
    for (size_t i = 0; i < data.Size(); ++i) {
        const Json::Value& item = data.At(i);
        Version version;
        version.id = std::to_string(item["id"].AsInt(0));
        version.name = item["displayName"].AsString();
        version.version_number = item["displayName"].AsString();

        const Json::Value& game_versions = item["gameVersions"];
        for (size_t k = 0; k < game_versions.Size(); ++k) {
            const std::string value = game_versions.At(k).AsString();
            if (value == "Forge") {
                version.loaders.push_back("forge");
            } else if (value == "Fabric") {
                version.loaders.push_back("fabric");
            } else if (value == "Quilt") {
                version.loaders.push_back("quilt");
            } else if (value == "NeoForge") {
                version.loaders.push_back("neoforge");
            } else {
                version.game_versions.push_back(value);
            }
        }

        File file;
        file.url = item["downloadUrl"].AsString();
        file.filename = item["fileName"].AsString();
        file.size = item["fileLength"].AsInt(0);
        const Json::Value& hashes = item["hashes"];
        for (size_t k = 0; k < hashes.Size(); ++k) {
            if (hashes.At(k)["algo"].AsInt(-1) == 1) {  // 1 = sha1
                file.sha1 = hashes.At(k)["value"].AsString();
            }
        }
        if (!file.url.empty()) version.files.push_back(file);
        if (!version.id.empty()) out.push_back(std::move(version));
    }
    return true;
}

}  // namespace versionsource
}  // namespace download
