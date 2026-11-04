#include "app/directory/Directory.h"

#include <filesystem>
#include <system_error>
#include <utility>

#include "app/json/JsonArena.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace {

bool LinkOrCopyFile(const std::string& source, const std::string& dest, std::string& error) {
    if (!tools::PathExists(source)) return false;
    if (tools::PathExists(dest)) return true;

    const size_t slash = dest.find_last_of("/\\");
    if (slash != std::string::npos) {
        std::string dir_error;
        if (!tools::EnsureDir(dest.substr(0, slash), dir_error)) {
            error = dir_error;
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::create_hard_link(tools::ToPath(source), tools::ToPath(dest), ec);
    if (ec) {
        ec.clear();
        std::filesystem::copy_file(tools::ToPath(source), tools::ToPath(dest),
                                   std::filesystem::copy_options::overwrite_existing, ec);
    }
    if (ec) {
        error = "链接/复制核心文件失败: " + source + " -> " + dest;
        return false;
    }
    return true;
}

bool LinkOrCopyTree(const std::string& source, const std::string& dest, std::string& error) {
    if (!tools::IsDirectory(source)) return true;
    std::string dir_error;
    if (!tools::EnsureDir(dest, dir_error)) {
        error = dir_error;
        return false;
    }
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(tools::ToPath(source), ec)) {
        if (ec) continue;
        const auto relative = std::filesystem::relative(entry.path(), tools::ToPath(source), ec);
        if (ec) continue;
        const auto u8 = relative.generic_u8string();
        const std::string target = tools::JoinPath(dest, std::string(u8.begin(), u8.end()));
        if (entry.is_directory()) {
            std::string ensure_error;
            if (!tools::EnsureDir(target, ensure_error)) {
                error = ensure_error;
                return false;
            }
        } else if (entry.is_regular_file()) {
            const auto source_u8 = entry.path().generic_u8string();
            const std::string source_path(source_u8.begin(), source_u8.end());
            if (!LinkOrCopyFile(source_path, target, error)) return false;
        }
    }
    return true;
}

}  // namespace

std::string Directory::CoresRoot() const { return tools::JoinPath(game_root_, "cores"); }

std::string Directory::CoreDir(const std::string& id) const {
    return tools::JoinPath(CoresRoot(), id);
}

std::string Directory::CoreJson(const std::string& id) const {
    return tools::JoinPath(CoreDir(id), id + ".json");
}

std::string Directory::CoreJar(const std::string& id) const {
    return tools::JoinPath(CoreDir(id), id + ".jar");
}

std::string Directory::CoreNatives(const std::string& id) const {
    return tools::JoinPath(CoreDir(id), id + "-natives");
}

bool Directory::EnsureCore(const std::string& core_id, const std::string& source_version,
                           std::string& error) const {
    if (core_id.empty() || source_version.empty()) return false;
    if (tools::PathExists(CoreJson(core_id))) return true;

    std::string dir_error;
    if (!tools::EnsureDir(CoreDir(core_id), dir_error)) {
        error = dir_error;
        return false;
    }

    if (tools::PathExists(VersionJson(source_version))) {
        if (!LinkOrCopyFile(VersionJson(source_version), CoreJson(core_id), error)) return false;
    }
    if (tools::PathExists(VersionJar(source_version))) {
        if (!LinkOrCopyFile(VersionJar(source_version), CoreJar(core_id), error)) return false;
    }
    if (tools::IsDirectory(NativesDir(source_version))) {
        if (!LinkOrCopyTree(NativesDir(source_version), CoreNatives(core_id), error)) return false;
    }

    auto j = JsonArena::Instance().Open(tools::JoinPath(CoreDir(core_id), "rml.json"));
    if (!j) {
        error = j.error();
        return false;
    }
    j["schema"] = Json::Value::Int(2);
    j["kind"] = Json::Value::String("core");
    j["id"] = Json::Value::String(core_id);
    j["name"] = Json::Value::String(core_id);
    j["minecraft"] = Json::Value::String(core_id);
    j["complete"] = Json::Value::Bool(true);
    j["state"] = Json::Value::String("committed");
    j["fresh"] = Json::Value::Bool(false);
    return j.commit(error);
}
