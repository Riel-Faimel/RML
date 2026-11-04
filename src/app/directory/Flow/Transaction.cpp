#include "app/directory/Flow/Transaction.h"

#include <filesystem>

#include "app/directory/Directory.h"
#include "app/json/JsonArena.h"
#include "app/tools/tools.h"

namespace directory {
namespace flow {

Transaction::Transaction(std::string game_dir, std::string id, std::string role,
                         std::string loader, std::string loader_version,
                         std::string minecraft, std::string base, bool user_installed)
    : game_dir_(std::move(game_dir)),
      id_(std::move(id)),
      name_(id_),
      role_(std::move(role)),
      loader_(std::move(loader)),
      loader_version_(std::move(loader_version)),
      minecraft_(std::move(minecraft)),
      base_(std::move(base)),
      user_installed_(user_installed) {
    const Directory paths(game_dir_);
    const std::string dir = paths.VersionDir(id_);
    const bool fresh = !tools::PathExists(dir);

    std::string error;
    if (!tools::EnsureDir(dir, error)) {
        error_ = error;
        return;
    }

    auto j = JsonArena::Instance().Open(dir + "/rml.json");
    if (!j) {
        error_ = j.error();
        return;
    }
    j["schema"] = Json::Value::Int(2);
    j["id"] = Json::Value::String(id_);
    j["name"] = Json::Value::String(name_);
    j["role"] = Json::Value::String(role_);
    j["loader"] = Json::Value::String(loader_);
    j["loaderVersion"] = Json::Value::String(loader_version_);
    j["minecraft"] = Json::Value::String(minecraft_);
    j["base"] = Json::Value::String(base_);
    j["complete"] = Json::Value::Bool(false);
    j["userInstalled"] = Json::Value::Bool(user_installed_);
    j["state"] = Json::Value::String("installing");
    j["fresh"] = Json::Value::Bool(fresh);
    if (!j.commit(error)) {
        error_ = error;
        return;
    }
    ok_ = true;
}

Transaction::~Transaction() {
    if (!ok_ || committed_) return;

    // 默认不保留 status=failed 的版本：失败安装/失败修复都整目录删除。
    const Directory paths(game_dir_);
    std::error_code ec;
    std::filesystem::remove_all(tools::ToPath(paths.VersionDir(id_)), ec);
}

bool Transaction::Commit(const std::string& name) {
    if (!ok_ || committed_) return false;
    if (!name.empty()) name_ = name;

    const Directory paths(game_dir_);
    auto j = JsonArena::Instance().Open(paths.VersionDir(id_) + "/rml.json");
    if (!j) {
        error_ = j.error();
        return false;
    }
    j["name"] = Json::Value::String(name_);
    j["complete"] = Json::Value::Bool(true);
    j["state"] = Json::Value::String("committed");
    j["fresh"] = Json::Value::Bool(false);
    std::string error;
    if (!j.commit(error)) {
        error_ = error;
        return false;
    }
    committed_ = true;
    return true;
}

}  // namespace flow
}  // namespace directory
