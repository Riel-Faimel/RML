#include "app/directory/Migration/Plan.h"

#include <algorithm>
#include <filesystem>

#include "app/directory/Directory.h"
#include "app/json/JsonArena.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace directory {
namespace migration {

namespace {

namespace fs = std::filesystem;

std::string ReadVersionField(const std::string& path, const std::string& key) {
    const std::string text = tools::ReadTextFile(path);
    if (text.empty()) return {};
    Json::Value root;
    Json json;
    std::string error;
    if (!json.Parse(text, root, error)) return {};
    return root[key].AsString();
}

bool SameFile(const std::string& left, const std::string& right) {
    if (!tools::PathExists(left) || !tools::PathExists(right)) return false;
    std::error_code ec;
    if (fs::file_size(tools::ToPath(left), ec) != fs::file_size(tools::ToPath(right), ec)) return false;
    std::string a;
    std::string b;
    return tools::Sha1File(left, a) && tools::Sha1File(right, b) &&
           tools::ToLower(a) == tools::ToLower(b);
}

bool CollectTree(const std::string& root, std::map<std::string, std::string>& out) {
    std::error_code ec;
    if (!fs::is_directory(tools::ToPath(root), ec)) return false;
    for (const auto& entry : fs::recursive_directory_iterator(tools::ToPath(root), ec)) {
        if (ec || !entry.is_regular_file()) continue;
        const auto relative = fs::relative(entry.path(), tools::ToPath(root), ec);
        if (ec) continue;
        const auto u8 = relative.generic_u8string();
        const auto path_u8 = entry.path().generic_u8string();
        std::string hash;
        if (!tools::Sha1File(std::string(path_u8.begin(), path_u8.end()), hash)) return false;
        out[std::string(u8.begin(), u8.end())] = tools::ToLower(hash);
    }
    return !ec;
}

bool SameTree(const std::string& left, const std::string& right) {
    std::map<std::string, std::string> a;
    std::map<std::string, std::string> b;
    return CollectTree(left, a) && CollectTree(right, b) && a == b;
}

CoreState InspectCore(const Directory& paths, const std::string& id) {
    CoreState state;
    state.exists = tools::IsDirectory(paths.CoreDir(id));
    if (!state.exists) return state;
    state.has_json = tools::PathExists(paths.CoreJson(id));
    state.has_jar = tools::PathExists(paths.CoreJar(id));
    state.has_natives = tools::IsDirectory(paths.CoreNatives(id));
    const std::string marker = paths.CoreDir(id) + "/rml.json";
    if (!tools::PathExists(marker)) return state;
    auto j = JsonArena::Instance().Open(marker);
    if (j) state.managed = j.Get("kind").AsString() == "core";
    return state;
}

std::string UniqueCow(const Directory& paths, const std::string& base, const Entry& entry) {
    std::string hash;
    if (entry.has_json) {
        tools::Sha1File(entry.dir + "/" + entry.id + ".json", hash);
    } else if (entry.has_jar) {
        tools::Sha1File(entry.dir + "/" + entry.id + ".jar", hash);
    }
    if (hash.size() < 8) hash = "00000000";
    const std::string stem = base + "-cov-" + tools::ToLower(hash.substr(0, 8));
    std::string candidate = stem;
    for (int suffix = 2; tools::PathExists(paths.CoreDir(candidate)); ++suffix) {
        candidate = stem + "-" + std::to_string(suffix);
    }
    return candidate;
}

void CopyExternalCore(const Entry& source, const std::string& core, const Directory& paths,
                      Plan& plan) {
    if (!source.has_json) return;
    plan.NewCore(core, source.minecraft);
    const std::string target = paths.CoreDir(core);
    plan.Link(source.dir + "/" + source.id + ".json", target + "/" + core + ".json",
              "复制外部版本 json 到核心: " + source.id);
    if (source.has_jar) {
        plan.Link(source.dir + "/" + source.id + ".jar", target + "/" + core + ".jar",
                  "复制外部版本 jar 到核心: " + source.id);
    }
    if (source.has_natives) {
        plan.Link(source.dir + "/" + source.id + "-natives", target + "/" + core + "-natives",
                  "复制外部版本 natives 到核心: " + source.id);
    }
}

}  // namespace

void Plan::Line(const std::string& text) { lines.push_back(text); }

void Plan::NewCore(const std::string& core, const std::string& minecraft) {
    if (!created.insert(core).second) return;
    Action action;
    action.kind = Kind::NewCore;
    action.a = core;
    action.minecraft = minecraft;
    actions.push_back(std::move(action));
    Line("新建核心: " + core);
    ++cores;
}

void Plan::Move(const std::string& from, const std::string& to, const std::string& label) {
    if (from.empty() || to.empty()) return;
    Action action;
    action.kind = Kind::Move;
    action.a = from;
    action.b = to;
    actions.push_back(std::move(action));
    Line(label);
    ++moved;
}

void Plan::Link(const std::string& from, const std::string& to, const std::string& label) {
    Action action;
    action.kind = Kind::Link;
    action.a = to;
    action.b = from;
    actions.push_back(std::move(action));
    Line(label);
    ++linked;
}

void Plan::Drop(const std::string& path, const std::string& source, const std::string& label) {
    Action action;
    action.kind = Kind::Drop;
    action.a = path;
    action.b = source;
    actions.push_back(std::move(action));
    Line(label);
    ++dropped;
}

void Plan::DropTree(const std::string& path, const std::string& source, const std::string& label) {
    Action action;
    action.kind = Kind::DropTree;
    action.a = path;
    action.b = source;
    actions.push_back(std::move(action));
    Line(label);
    ++dropped;
}

void Plan::Marker(const Entry& entry, const std::string& target) {
    Action action;
    action.kind = Kind::Marker;
    action.a = entry.dir + "/rml.json";
    action.b = entry.core;
    action.core = target;
    action.entry = entry.id;
    actions.push_back(std::move(action));
    Line("写入入口 core: " + entry.id + " -> " + target);
    ++markers;
}

void Plan::Skip(const std::string& text) {
    Line("跳过: " + text);
    ++skipped;
}

bool LoadEntry(const Directory& paths, const std::string& id, Entry& entry) {
    entry.id = id;
    entry.dir = paths.VersionDir(id);
    const std::string marker = entry.dir + "/rml.json";
    entry.managed = tools::PathExists(marker);
    entry.has_json = tools::PathExists(entry.dir + "/" + id + ".json");
    entry.has_jar = tools::PathExists(entry.dir + "/" + id + ".jar");
    entry.has_natives = tools::IsDirectory(entry.dir + "/" + id + "-natives");
    if (!entry.managed) {
        entry.external = true;
        entry.minecraft = ReadVersionField(entry.dir + "/" + id + ".json", "inheritsFrom");
        if (entry.minecraft.empty()) entry.minecraft = id;
        return true;
    }

    auto j = JsonArena::Instance().Open(marker);
    if (!j) return false;
    entry.role = j.Get("role").AsString();
    entry.state = j.Get("state").AsString();
    entry.base = j.Get("base").AsString();
    entry.minecraft = j.Get("minecraft").AsString();
    entry.core = j.Get("core").AsString();
    entry.complete = j.Get("complete").AsBool(false);
    if (!Directory::ValidCoreId(entry.base)) entry.base.clear();
    if (!Directory::ValidCoreId(entry.minecraft)) entry.minecraft.clear();
    if (!Directory::ValidCoreId(entry.core)) entry.core.clear();
    entry.server = entry.role == "server";
    entry.loader = entry.role == "loader";
    if (entry.minecraft.empty()) entry.minecraft = id;
    return true;
}

void PlanVanilla(const Entry& entry, const Directory& paths, Plan& plan) {
    if (!entry.has_json && !entry.has_jar) {
        plan.Skip(entry.id + "：没有版本 json/jar，入口保持原状");
        return;
    }
    const std::string candidate = entry.core.empty() ? entry.id : entry.core;
    CoreState state = InspectCore(paths, candidate);

    bool cow = state.exists && !state.managed;
    if (state.exists && state.managed) {
        if (entry.has_json && state.has_json &&
            !SameFile(entry.dir + "/" + entry.id + ".json", paths.CoreJson(candidate))) {
            cow = true;
        }
        if (entry.has_jar && state.has_jar &&
            !SameFile(entry.dir + "/" + entry.id + ".jar", paths.CoreJar(candidate))) {
            cow = true;
        }
        if (entry.has_natives && state.has_natives &&
            !SameTree(entry.dir + "/" + entry.id + "-natives", paths.CoreNatives(candidate))) {
            cow = true;
        }
    }

    std::string target = candidate;
    const bool had_core_json = state.managed && state.has_json;
    if (!entry.has_json && !had_core_json) {
        plan.Skip(entry.id + "：入口和旧核心都没有 json，保留现场不迁移");
        return;
    }
    if (cow) {
        target = UniqueCow(paths, candidate, entry);
        state = CoreState{};
        plan.Line("核心内容与入口不一致，改用新核心: " + target);
    }
    if (!state.exists) plan.NewCore(target, entry.minecraft);
    plan.remap[entry.id] = target;

    const std::string core_dir = paths.CoreDir(target);
    const std::string core_json = core_dir + "/" + target + ".json";
    const std::string core_jar = core_dir + "/" + target + ".jar";
    const std::string core_natives = core_dir + "/" + target + "-natives";

    if (entry.has_json) {
        if (tools::PathExists(core_json)) {
            if (SameFile(entry.dir + "/" + entry.id + ".json", core_json)) {
                plan.Drop(entry.dir + "/" + entry.id + ".json", core_json,
                          "去重入口 json（核心已有相同内容）: " + entry.id);
            } else {
                plan.Skip(entry.id + "：入口 json 与目标核心不同，保留入口文件");
            }
        } else {
            plan.Move(entry.dir + "/" + entry.id + ".json", core_json, "移入核心 json: " + entry.id);
        }
    } else if (had_core_json && cow) {
        plan.Link(paths.CoreJson(candidate), core_json, "为新核心复制 json: " + target);
    }

    if (entry.has_jar) {
        if (tools::PathExists(core_jar)) {
            if (SameFile(entry.dir + "/" + entry.id + ".jar", core_jar)) {
                plan.Drop(entry.dir + "/" + entry.id + ".jar", core_jar,
                          "去重入口 jar（核心已有相同内容）: " + entry.id);
            } else {
                plan.Skip(entry.id + "：入口 jar 与核心不同，保留入口文件");
            }
        } else {
            plan.Move(entry.dir + "/" + entry.id + ".jar", core_jar, "移入核心 jar: " + entry.id);
        }
    }

    if (entry.has_natives) {
        if (tools::IsDirectory(core_natives)) {
            if (SameTree(entry.dir + "/" + entry.id + "-natives", core_natives)) {
                plan.DropTree(entry.dir + "/" + entry.id + "-natives", core_natives,
                              "去重入口 natives（核心已有相同内容）: " + entry.id);
            } else {
                plan.Skip(entry.id + "：入口 natives 与核心不同，保留入口目录");
            }
        } else {
            plan.Move(entry.dir + "/" + entry.id + "-natives", core_natives,
                      "移入核心 natives: " + entry.id);
        }
    }

    if (entry.core != target) plan.Marker(entry, target);
}

void PlanLoader(const Entry& entry, const Directory& paths,
                const std::map<std::string, Entry>& entries, Plan& plan) {
    const std::string requested = entry.core.empty()
                                      ? (entry.base.empty() ? entry.minecraft : entry.base)
                                      : entry.core;
    std::string core = requested;
    auto mapped = plan.remap.find(core);
    if (mapped != plan.remap.end()) core = mapped->second;

    CoreState state = InspectCore(paths, core);
    const bool created = plan.created.count(core) > 0;
    if ((!state.exists || !state.managed || !state.has_json) && !created) {
        auto source = entries.find(requested);
        if (source != entries.end() && source->second.external && source->second.has_json) {
            CopyExternalCore(source->second, core, paths, plan);
        } else {
            plan.Skip(entry.id + "：核心 " + core + " 尚不存在，入口继续使用自身文件");
            return;
        }
    }

    auto base = entries.find(requested);
    const bool base_planned = plan.created.count(core) > 0;
    const bool core_has_jar =
        state.has_jar || (base_planned && base != entries.end() && base->second.has_jar);
    const bool core_has_natives =
        state.has_natives || (base_planned && base != entries.end() && base->second.has_natives);

    if (entry.has_jar) {
        const std::string core_jar = paths.CoreJar(core);
        if (core_has_jar) {
            const std::string reference =
                state.has_jar || base == entries.end()
                    ? core_jar
                    : base->second.dir + "/" + base->second.id + ".jar";
            if (SameFile(entry.dir + "/" + entry.id + ".jar", reference)) {
                plan.Drop(entry.dir + "/" + entry.id + ".jar", core_jar,
                          "去重加载器入口里的继承 jar: " + entry.id);
            } else {
                plan.Skip(entry.id + "：加载器入口 jar 与核心不同，保留");
            }
        } else {
            plan.Move(entry.dir + "/" + entry.id + ".jar", core_jar,
                      "继承 jar 移入核心: " + entry.id);
        }
    }

    if (entry.has_natives) {
        const std::string core_natives = paths.CoreNatives(core);
        if (core_has_natives) {
            const std::string reference =
                state.has_natives || base == entries.end()
                    ? core_natives
                    : base->second.dir + "/" + base->second.id + "-natives";
            if (SameTree(entry.dir + "/" + entry.id + "-natives", reference)) {
                plan.DropTree(entry.dir + "/" + entry.id + "-natives", core_natives,
                              "去重加载器入口里的继承 natives: " + entry.id);
            } else {
                plan.Skip(entry.id + "：加载器入口 natives 与核心不同，保留");
            }
        } else {
            plan.Move(entry.dir + "/" + entry.id + "-natives", core_natives,
                      "继承 natives 移入核心: " + entry.id);
        }
    }

    if (entry.core != core) plan.Marker(entry, core);
}

void PlanServer(const Entry& entry, const Directory& paths,
                const std::map<std::string, Entry>& entries, Plan& plan) {
    const std::string requested =
        entry.core.empty() ? (entry.minecraft.empty() ? entry.id : entry.minecraft) : entry.core;
    std::string core = requested;
    auto mapped = plan.remap.find(core);
    if (mapped != plan.remap.end()) core = mapped->second;

    CoreState state = InspectCore(paths, core);
    if ((!state.exists || !state.has_json) && !plan.created.count(core)) {
        auto source = entries.find(requested);
        if (source != entries.end() && source->second.external && source->second.has_json) {
            CopyExternalCore(source->second, core, paths, plan);
        } else if (!tools::PathExists(paths.CoreJson(core))) {
            plan.Skip(entry.id + "：服务端安装引用的核心 " + core +
                      " 不存在（server.jar 留在入口）");
        }
    }
    if (entry.core != core) plan.Marker(entry, core);
}

}  // namespace migration
}  // namespace directory
