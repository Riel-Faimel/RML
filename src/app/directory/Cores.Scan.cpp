#include "app/directory/Directory.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <system_error>

#include "app/json/JsonArena.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace {

std::string DefaultCore(const std::string& id, const std::string& role, const std::string& base,
                        const std::string& minecraft) {
    if (role == "loader") {
        if (!base.empty()) return base;
        if (!minecraft.empty()) return minecraft;
    }
    if (role == "server" && !minecraft.empty()) return minecraft;
    return id;
}

int64_t TreeBytes(const std::string& root) {
    int64_t total = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(tools::ToPath(root), ec)) {
        if (ec || !entry.is_regular_file()) continue;
        std::error_code size_ec;
        const auto size = entry.file_size(size_ec);
        if (!size_ec) total += static_cast<int64_t>(size);
    }
    return total;
}

// 重新数一遍引用，避免使用可能过期的容器做删除判断。
int CountCoreRefs(const std::string& game_root, const std::string& core_id) {
    const std::string versions = tools::JoinPath(game_root, "versions");
    int refs = 0;
    for (const std::string& id : tools::ListDirectories(versions)) {
        const std::string marker = tools::JoinPath(tools::JoinPath(versions, id), "rml.json");
        if (!tools::PathExists(marker)) continue;
        auto j = JsonArena::Instance().Open(marker);
        if (!j) continue;
        const std::string role = j.Get("role").AsString();
        if (role == "external") continue;
        std::string core = j.Get("core").AsString();
        if (core.empty()) {
            core = DefaultCore(id, role, j.Get("base").AsString(), j.Get("minecraft").AsString());
        }
        if (core == core_id) ++refs;
    }
    return refs;
}

}  // namespace

void Directory::RefreshCores() {
    cores_.clear();
    std::map<std::string, int> refs;

    const std::string versions = VersionsRoot();
    for (const std::string& id : tools::ListDirectories(versions)) {
        const std::string marker = tools::JoinPath(tools::JoinPath(versions, id), "rml.json");
        if (!tools::PathExists(marker)) continue;
        auto j = JsonArena::Instance().Open(marker);
        if (!j) continue;
        const std::string role = j.Get("role").AsString();
        if (role == "external") continue;
        std::string core = j.Get("core").AsString();
        if (core.empty()) {
            core = DefaultCore(id, role, j.Get("base").AsString(), j.Get("minecraft").AsString());
        }
        if (!core.empty()) ++refs[core];
    }

    for (const std::string& id : tools::ListDirectories(CoresRoot())) {
        if (id.empty() || id.front() == '.') continue;  // .migration 等内部目录不是核心
        const std::string dir = CoreDir(id);
        CoreEntry core;
        core.id = id;
        core.directory = dir;
        core.bytes = TreeBytes(dir);

        const std::string marker = tools::JoinPath(dir, "rml.json");
        if (tools::PathExists(marker)) {
            auto j = JsonArena::Instance().Open(marker);
            if (!j) {
                core.valid = false;
            } else {
                core.valid = j.Get("kind").AsString() == "core";
            }
        } else {
            core.valid = false;
        }

        const auto found = refs.find(core.id);
        core.refs = found == refs.end() ? 0 : found->second;
        cores_.push_back(std::move(core));
    }

    std::sort(cores_.begin(), cores_.end(),
              [](const CoreEntry& left, const CoreEntry& right) { return left.id < right.id; });
}

const Directory::CoreEntry* Directory::FindCore(const std::string& id) const {
    for (const auto& core : cores_) {
        if (core.id == id) return &core;
    }
    return nullptr;
}

std::string Directory::CoreForEntry(const std::string& id) const {
    for (const auto& entry : versions_) {
        if (entry.id == id && !entry.core.empty() && ValidCoreId(entry.core)) return entry.core;
    }
    // 安装器会 new 一个临时 Directory，此时 versions_ 还是空的；直接看 rml。
    const std::string marker = tools::JoinPath(VersionDir(id), "rml.json");
    if (tools::PathExists(marker)) {
        auto j = JsonArena::Instance().Open(marker);
        if (j) {
            const std::string core = j.Get("core").AsString();
            if (!core.empty() && ValidCoreId(core)) return core;
        }
    }
    return id;
}

bool Directory::ValidCoreId(const std::string& id) {
    if (id.empty() || id == "." || id == "..") return false;
    if (id.find("..") != std::string::npos) return false;
    return id.find('/') == std::string::npos && id.find('\\') == std::string::npos &&
           id.find(':') == std::string::npos;
}

bool Directory::RemoveCore(const std::string& id, std::string& error) const {
    if (id.empty() || id.find('/') != std::string::npos || id.find('\\') != std::string::npos) {
        error = "核心 id 非法: " + id;
        return false;
    }
    const std::string dir = CoreDir(id);
    if (!tools::IsDirectory(dir)) {
        error = "核心不存在: " + dir;
        return false;
    }

    const std::string marker = tools::JoinPath(dir, "rml.json");
    if (!tools::PathExists(marker)) {
        error = "拒绝删除不受 RML 管理的目录: " + dir;
        return false;
    }
    {
        auto j = JsonArena::Instance().Open(marker);
        if (!j || j.Get("kind").AsString() != "core") {
            error = "拒绝删除不受 RML 管理的目录: " + dir;
            return false;
        }
    }

    const int refs = CountCoreRefs(game_root_, id);
    if (refs > 0) {
        error = "核心仍被 " + std::to_string(refs) + " 个版本入口引用: " + id;
        return false;
    }

    std::error_code ec;
    std::filesystem::remove_all(tools::ToPath(dir), ec);
    if (ec) {
        error = "删除核心失败: " + dir + "（" + ec.message() + "）";
        return false;
    }
    return true;
}

int Directory::RemoveOrphanCores(std::string& error) const {
    error.clear();
    std::vector<std::string> ids;
    for (const auto& core : cores_) {
        if (core.Orphan()) ids.push_back(core.id);
    }

    int removed = 0;
    for (const auto& id : ids) {
        std::string core_error;
        if (!RemoveCore(id, core_error)) {
            if (!error.empty()) error += "\n";
            error += core_error;
            continue;
        }
        ++removed;
    }
    return removed;
}
