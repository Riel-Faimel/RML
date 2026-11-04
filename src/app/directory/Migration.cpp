#include "app/directory/Directory.h"

#include <filesystem>
#include <map>
#include <set>
#include <system_error>
#include <vector>

#include "app/directory/Migration/Journal.h"
#include "app/directory/Migration/Plan.h"
#include "app/json/JsonArena.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace {

namespace fs = std::filesystem;

bool CopyLinkFile(const std::string& source, const std::string& dest, std::string& error) {
    const size_t slash = dest.find_last_of("/\\");
    if (slash != std::string::npos) {
        std::string dir_error;
        if (!tools::EnsureDir(dest.substr(0, slash), dir_error)) {
            error = dir_error;
            return false;
        }
    }
    std::error_code ec;
    fs::create_hard_link(tools::ToPath(source), tools::ToPath(dest), ec);
    if (ec) {
        ec.clear();
        fs::copy_file(tools::ToPath(source), tools::ToPath(dest),
                      fs::copy_options::overwrite_existing, ec);
    }
    if (ec) {
        error = "复制/硬链接失败: " + source + " -> " + dest;
        return false;
    }
    return true;
}

bool CopyLinkEntry(const std::string& source, const std::string& dest, std::string& error) {
    if (!tools::IsDirectory(source)) return CopyLinkFile(source, dest, error);
    std::string dir_error;
    if (!tools::EnsureDir(dest, dir_error)) {
        error = dir_error;
        return false;
    }
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(tools::ToPath(source), ec)) {
        if (ec || !entry.is_regular_file()) continue;
        const auto relative = fs::relative(entry.path(), tools::ToPath(source), ec);
        if (ec) continue;
        const auto u8 = relative.generic_u8string();
        const auto path_u8 = entry.path().generic_u8string();
        const std::string target = tools::JoinPath(dest, std::string(u8.begin(), u8.end()));
        if (!CopyLinkFile(std::string(path_u8.begin(), path_u8.end()), target, error)) return false;
    }
    if (ec) {
        error = "复制目录失败: " + source;
        return false;
    }
    return true;
}

void ReportFailure(const std::string& game_root, directory::migration::Journal& journal,
                   Directory::MigrationReport& report, const std::string& error) {
    report.error = error;
    report.lines.push_back("迁移失败: " + error);

    std::string rolled;
    std::string rollback_error;
    if (directory::migration::Journal::RollbackRun(game_root, journal.Dir(), rolled,
                                                   rollback_error)) {
        report.lines.push_back("已自动回滚本次迁移，现场保留在: " + journal.Dir());
    } else {
        std::string ignored;
        journal.WriteState("failed", ignored);
        report.lines.push_back("自动回滚失败: " + rollback_error + "（日志: " + journal.Dir() + "）");
    }
}

}  // namespace

bool Directory::MigrateCores(bool dry_run, MigrationReport& report,
                             const std::string& only_entry) const {
    namespace migration = directory::migration;
    report = MigrationReport{};
    report.dry_run = dry_run;

    std::map<std::string, migration::Entry> entries;
    for (const std::string& id : tools::ListDirectories(VersionsRoot())) {
        migration::Entry entry;
        if (!migration::LoadEntry(*this, id, entry)) continue;
        entries.emplace(id, std::move(entry));
    }

    std::set<std::string> wanted;
    if (!only_entry.empty()) {
        wanted.insert(only_entry);
        auto found = entries.find(only_entry);
        if (found != entries.end()) {
            const auto& entry = found->second;
            if (!entry.base.empty()) wanted.insert(entry.base);
            if (!entry.minecraft.empty()) wanted.insert(entry.minecraft);
            if (!entry.core.empty()) wanted.insert(entry.core);
        }
    }

    std::vector<const migration::Entry*> vanilla;
    std::vector<const migration::Entry*> loaders;
    std::vector<const migration::Entry*> servers;
    for (const auto& pair : entries) {
        const auto& entry = pair.second;
        if (!wanted.empty() && wanted.count(entry.id) == 0) continue;
        if (entry.external) continue;
        if (entry.state != "committed" || !entry.complete) {
            report.lines.push_back("跳过: " + entry.id + "：安装未提交");
            ++report.skipped;
            continue;
        }
        if (entry.server) servers.push_back(&entry);
        else if (entry.loader) loaders.push_back(&entry);
        else vanilla.push_back(&entry);
    }

    migration::Plan plan;
    for (const auto* entry : vanilla) migration::PlanVanilla(*entry, *this, plan);
    for (const auto* entry : loaders) migration::PlanLoader(*entry, *this, entries, plan);
    for (const auto* entry : servers) migration::PlanServer(*entry, *this, entries, plan);

    report.lines.insert(report.lines.end(), plan.lines.begin(), plan.lines.end());
    report.moved = plan.moved;
    report.linked = plan.linked;
    report.dropped = plan.dropped;
    report.markers = plan.markers;
    report.skipped += plan.skipped;
    report.cores_created = plan.cores;

    if (plan.actions.empty()) {
        report.ok = true;
        report.lines.push_back("没有需要迁移的核心文件");
        return true;
    }

    if (dry_run) {
        report.ok = true;
        report.lines.push_back("预演完成：未改动任何文件");
        return true;
    }

    migration::Journal journal;
    std::string error;
    if (!journal.Begin(CoresRoot(), error)) {
        report.error = error;
        report.lines.push_back("迁移失败: " + error);
        return false;
    }
    report.journal = journal.Dir();

    // 元数据备份：写 core 之前先复制入口 rml，失败也能人工对照。
    for (const auto& action : plan.actions) {
        if (action.kind != migration::Kind::Marker) continue;
        const std::string dir =
            tools::JoinPath(migration::Journal::BackupDir(journal.Dir()), action.entry);
        std::string dir_error;
        if (!tools::EnsureDir(dir, dir_error)) {
            ReportFailure(game_root_, journal, report, dir_error);
            return false;
        }
        std::error_code ec;
        fs::copy_file(tools::ToPath(action.a), tools::ToPath(dir + "/rml.json.bak"),
                      fs::copy_options::overwrite_existing, ec);
        if (ec) {
            ReportFailure(game_root_, journal, report, "备份 rml 失败: " + action.a);
            return false;
        }
    }

    for (const auto& action : plan.actions) {
        if (action.kind == migration::Kind::NewCore) {
            const std::string core_dir = CoreDir(action.a);
            if (tools::PathExists(core_dir)) {
                error = "目标核心目录已存在，拒绝覆盖: " + core_dir;
                break;
            }
            if (!journal.Record("newcore", core_dir, std::string(), error)) break;
            std::string dir_error;
            if (!tools::EnsureDir(core_dir, dir_error)) {
                error = dir_error;
                break;
            }
            auto j = JsonArena::Instance().Open(core_dir + "/rml.json");
            if (!j) {
                error = j.error();
                break;
            }
            j["schema"] = Json::Value::Int(2);
            j["kind"] = Json::Value::String("core");
            j["id"] = Json::Value::String(action.a);
            j["name"] = Json::Value::String(action.a);
            j["minecraft"] = Json::Value::String(action.minecraft);
            j["complete"] = Json::Value::Bool(true);
            j["state"] = Json::Value::String("committed");
            j["fresh"] = Json::Value::Bool(false);
            if (!j.commit(error)) break;
            continue;
        }

        if (action.kind == migration::Kind::Marker) {
            if (!journal.Record("marker", action.a, action.b, error)) break;
            auto j = JsonArena::Instance().Open(action.a);
            if (!j) {
                error = j.error();
                break;
            }
            j["core"] = Json::Value::String(action.core);
            if (!j.commit(error)) break;
            continue;
        }

        const std::string kind =
            action.kind == migration::Kind::Move
                ? "move"
                : (action.kind == migration::Kind::Link
                       ? "link"
                       : (action.kind == migration::Kind::Drop ? "drop" : "dropdir"));
        if (!journal.Record(kind, action.a, action.b, error)) break;

        if (action.kind == migration::Kind::Link) {
            if (!CopyLinkEntry(action.b, action.a, error)) break;
            continue;
        }
        if (action.kind == migration::Kind::Move) {
            const size_t slash = action.b.find_last_of("/\\");
            std::string dir_error;
            if (slash == std::string::npos || !tools::EnsureDir(action.b.substr(0, slash), dir_error)) {
                error = dir_error;
                break;
            }
            if (tools::PathExists(action.b)) {
                error = "迁移目标已存在，拒绝覆盖: " + action.b;
                break;
            }
            std::error_code ec;
            fs::rename(tools::ToPath(action.a), tools::ToPath(action.b), ec);
            if (ec) {
                ec.clear();
                if (!CopyLinkFile(action.a, action.b, error)) break;
                fs::remove_all(tools::ToPath(action.a), ec);
                if (ec) {
                    // 源文件还在时不要把复制品留下，否则回滚会看到两边都存在。
                    if (tools::PathExists(action.a)) {
                        std::error_code ignored;
                        fs::remove_all(tools::ToPath(action.b), ignored);
                    }
                    error = "移动后删除入口文件失败: " + action.a;
                    break;
                }
            }
            continue;
        }
        std::error_code ec;
        fs::remove_all(tools::ToPath(action.a), ec);
        if (ec) {
            error = "删除入口文件失败: " + action.a;
            break;
        }
    }

    if (!error.empty()) {
        ReportFailure(game_root_, journal, report, error);
        return false;
    }
    if (!journal.WriteState("committed", error)) {
        ReportFailure(game_root_, journal, report, error);
        return false;
    }

    report.ok = true;
    report.lines.push_back("迁移完成：移动 " + std::to_string(report.moved) + "，复制 " +
                           std::to_string(report.linked) + "，去重 " +
                           std::to_string(report.dropped) + "，新核心 " +
                           std::to_string(report.cores_created) + "，写入 core " +
                           std::to_string(report.markers));
    return true;
}

bool Directory::RollbackLastMigration(std::string& error, std::string& report) const {
    const bool ok = directory::migration::Journal::RollbackLast(game_root_, CoresRoot(), report, error);
    return ok;
}
