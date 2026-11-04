#include "app/directory/Directory.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <utility>

#include "app/json/JsonArena.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace {

bool ValidRole(const std::string& role) {
    return role == "game" || role == "loader" || role == "base" || role == "server" ||
           role == "external";
}

bool ValidState(const std::string& state) {
    return state == "installing" || state == "committed" || state == "failed";
}

bool ReadJsonFile(const std::string& path, Json::Value& out) {
    std::ifstream stream(tools::ToPath(path), std::ios::binary);
    if (!stream) return false;
    const std::string text((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    Json json;
    std::string error;
    return !text.empty() && json.Parse(text, out, error);
}

}  // namespace

void Directory::RefreshVersions() {
    versions_.clear();
    visible_.clear();
    dependency_owners_.clear();

    std::error_code ec;
    std::vector<std::string> ids;
    for (const auto& entry : std::filesystem::directory_iterator(tools::ToPath(VersionsRoot()), ec)) {
        if (ec || !entry.is_directory()) continue;
        const auto name = entry.path().filename().u8string();
        ids.emplace_back(name.begin(), name.end());
    }
    std::sort(ids.begin(), ids.end());

    for (const auto& id : ids) {
        VersionEntry version;
        version.id = id;
        version.name = id;
        version.directory = VersionDir(id);
        version.has_json = std::filesystem::is_regular_file(tools::ToPath(VersionJson(id)));

        const std::string marker_path = version.directory + "/rml.json";
        if (!tools::PathExists(marker_path)) {
            // 没有 rml.json 的是外部版本；它不受事务和依赖管理，核心文件也不搬。
            if (!version.has_json) continue;
            Json::Value root;
            if (!ReadJsonFile(VersionJson(id), root) || root["id"].AsString().empty()) continue;
            version.role = "game";
            version.state = "committed";
            version.user_installed = true;
            version.minecraft_version = root["inheritsFrom"].AsString();
            if (version.minecraft_version.empty()) version.minecraft_version = id;
            version.has_jar = std::filesystem::is_regular_file(tools::ToPath(VersionJar(id)));
            versions_.push_back(std::move(version));
            continue;
        }

        // 入口的 rml.json 是唯一事实来源；以下内容都在代理作用域内读完。
        std::string role;
        std::string state;
        std::string base;
        std::string loader;
        std::string minecraft;
        std::string name;
        std::string meta_id;
        std::string core;
        bool complete = false;
        bool user = true;
        bool legacy = false;
        bool marker_ok = false;

        {
            auto j = JsonArena::Instance().Open(marker_path);
            if (!j) {
                version.managed = true;
                version.valid = false;
                versions_.push_back(std::move(version));
                continue;
            }

            const int schema = static_cast<int>(j.Get("schema").AsInt(0));
            if (schema > 2) {
                version.managed = true;
                version.valid = false;
                versions_.push_back(std::move(version));
                continue;
            }

            legacy = schema < 2;
            if (!legacy) {
                role = j.Get("role").AsString();
                state = j.Get("state").AsString();
                base = j.Get("base").AsString();
                loader = j.Get("loader").AsString();
                minecraft = j.Get("minecraft").AsString();
                name = j.Get("name").AsString();
                meta_id = j.Get("id").AsString();
                complete = j.Get("complete").AsBool(false);
                user = j.Get("userInstalled").AsBool(false);
                core = j.Get("core").AsString();
            } else {
                const bool server = j.Get("kind").AsString() == "server";
                const bool has_user = !j.Get("user").IsNull();
                user = has_user ? j.Get("user").AsBool(true) : true;
                role = server ? "server" : "game";
                loader = j.Get("loader").AsString();
                if (!server && loader == "原版" && !user) role = "base";
                if (!server && !loader.empty() && loader != "原版") role = "loader";

                const bool external = j.Get("managed").IsNull() && j.Get("loader").IsNull() &&
                                      j.Get("complete").IsNull() && j.Get("kind").IsNull();
                if (external) {
                    role = "external";
                    complete = true;
                    state = "committed";
                    user = true;
                } else {
                    complete = j.Get("complete").AsBool(false);
                    state = complete ? "committed" : "installing";
                }

                minecraft = j.Get("minecraft").AsString();
                if (minecraft.empty() && version.has_json) {
                    Json::Value legacy_root;
                    if (ReadJsonFile(VersionJson(id), legacy_root)) {
                        minecraft = legacy_root["inheritsFrom"].AsString();
                    }
                }
                if (minecraft.empty()) minecraft = id;
                name = j.Get("name").AsString();
                if (name.empty()) name = id;
                if ((role == "game" || role == "base") && loader.empty()) loader = "原版";
                if (role == "loader" && base.empty()) base = minecraft;

                if (role != "server") {
                    j["schema"] = Json::Value::Int(2);
                    j["id"] = Json::Value::String(id);
                    j["name"] = Json::Value::String(name);
                    j["role"] = Json::Value::String(role);
                    j["loader"] = Json::Value::String(loader);
                    j["loaderVersion"] = Json::Value::String(j.Get("loaderVersion").AsString());
                    j["minecraft"] = Json::Value::String(minecraft);
                    j["base"] = Json::Value::String(base);
                    j["complete"] = Json::Value::Bool(complete);
                    j["userInstalled"] = Json::Value::Bool(user);
                    j["state"] = Json::Value::String(state);
                    j["fresh"] = Json::Value::Bool(false);
                    j.remove("managed");
                    j.remove("kind");
                    j.remove("user");
                    std::string migrate_error;
                    j.commit(migrate_error);
                }
            }
            marker_ok = true;
        }

        if (!base.empty() && !ValidCoreId(base)) base.clear();
        if (!minecraft.empty() && !ValidCoreId(minecraft)) minecraft.clear();
        if (core.empty() || !ValidCoreId(core)) {
            if (role == "loader") core = !base.empty() ? base : minecraft;
            else if (role == "server") core = !minecraft.empty() ? minecraft : id;
            else core = id;
        }
        if (core.empty() || !ValidCoreId(core)) core = id;

        // 入口自己的版本描述可能已经搬进 cores；服务端安装没有版本描述。
        const bool core_json_exists =
            !core.empty() && std::filesystem::is_regular_file(tools::ToPath(CoreJson(core)));
        std::string json_path = version.directory + "/" + id + ".json";
        bool profile_exists = version.has_json;
        if (!profile_exists) {
            if (core_json_exists) {
                json_path = CoreJson(core);
                profile_exists = true;
            } else if (std::filesystem::is_regular_file(tools::ToPath(CoreJson(id)))) {
                json_path = CoreJson(id);
                profile_exists = true;
            }
        }

        Json::Value version_root;
        bool version_ok = false;
        if (profile_exists) version_ok = ReadJsonFile(json_path, version_root);
        if (version_ok) {
            const std::string root_id = version_root["id"].AsString();
            if (root_id.empty()) version_ok = false;
        }

        version.minecraft_version = minecraft;
        if (version.minecraft_version.empty()) {
            version.minecraft_version = version_root["inheritsFrom"].AsString();
        }
        if (version.minecraft_version.empty()) version.minecraft_version = id;

        if (!profile_exists && role != "server") {
            // 既没有入口 json 也没有核心 json：坏版本，补一条让用户能看到/删除。
            version.managed = true;
            version.valid = false;
            version.role = role.empty() ? "game" : role;
            version.state = state;
            version.core = core;
            version.has_json = false;
            version.has_core = false;
            version.loader = loader;
            version.base = base;
            version.is_server = role == "server";
            versions_.push_back(std::move(version));
            continue;
        }

        version.has_json = profile_exists;
        version.has_jar = std::filesystem::is_regular_file(tools::ToPath(VersionJar(id))) ||
                         std::filesystem::is_regular_file(tools::ToPath(CoreJar(core)));
        version.has_core = core_json_exists;

        bool valid = marker_ok;
        if (!ValidRole(role) || !ValidState(state)) valid = false;
        if (state == "committed" && !complete && role != "external") valid = false;
        if (role != "server" && role != "external" && version.minecraft_version.empty()) valid = false;
        if ((role == "game" || role == "base") && loader.empty()) valid = false;
        if (role == "loader" && (loader.empty() || base.empty())) valid = false;
        if (role == "external" && (state != "committed" || !complete)) valid = false;
        if (!legacy && meta_id.empty()) valid = false;
        if (!legacy && meta_id != id) valid = false;
        if (profile_exists && !version_ok) valid = false;
        if (role == "loader" && core.empty()) valid = false;

        version.managed = role != "external";
        version.core = core;
        version.role = role;
        version.state = state;
        version.base = base;
        version.fresh = false;
        version.loader = loader;
        version.complete = complete;
        version.user_installed = user;
        version.is_server = role == "server";
        version.dependency = role == "base";
        version.valid = valid;
        if (role == "external") {
            version.user_installed = true;
            version.complete = true;
            version.state = "committed";
        }
        if (!name.empty()) version.name = name;

        versions_.push_back(std::move(version));
    }

    // 加载器版本显式写 base；不再从 id 的 -forge-/-neoforge- 后缀猜。
    std::map<std::string, std::string> owners;
    for (const auto& version : versions_) {
        if (!version.managed || !version.valid || !version.complete ||
            version.state != "committed" || version.is_server || version.base.empty()) {
            continue;
        }
        auto found = owners.find(version.base);
        if (found == owners.end() || version.id > found->second) owners[version.base] = version.id;
    }

    for (auto& version : versions_) {
        const auto owner = owners.find(version.id);
        if (!version.valid || version.is_server) continue;
        if (version.role == "base" || version.dependency) {
            version.dependency = true;
            if (owner != owners.end()) dependency_owners_[version.id] = owner->second;
            continue;
        }
        // 安装中的版本也保留在 Entries 里，安装器 Refresh/Select 才能找到自己；
        // 启动页由 BridgeVersions 再按 complete 过滤。
        visible_.push_back(version);
    }

    RefreshCores();
}

void Directory::RecoverTransactions() {
    std::error_code ec;
    const auto root = tools::ToPath(VersionsRoot());
    if (!std::filesystem::is_directory(root, ec)) return;

    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (ec || !entry.is_directory()) continue;
        const auto name_u8 = entry.path().generic_u8string();
        const std::string dir(name_u8.begin(), name_u8.end());
        const std::string rml = tools::JoinPath(dir, "rml.json");
        bool remove = false;
        {
            auto j = JsonArena::Instance().Open(rml);
            if (!j) continue;
            if (j.Get("schema").AsInt(0) < 2) continue;
            if (j.Get("state").AsString() == "committed") continue;
            remove = true;
        }

        if (remove) {
            // 默认不保留 status=failed / installing 的版本；进程中断的残留也在这里清掉。
            std::error_code remove_ec;
            std::filesystem::remove_all(entry.path(), remove_ec);
        }
    }
}

std::string Directory::DependencyOwner(const std::string& id) const {
    const auto found = dependency_owners_.find(id);
    return found == dependency_owners_.end() ? std::string() : found->second;
}
