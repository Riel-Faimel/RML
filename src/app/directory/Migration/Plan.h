// 迁移计划：只读扫描 + 生成动作，不碰文件；执行与 WAL 在 Migration.cpp。
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

class Directory;

namespace directory {
namespace migration {

struct Entry {
    std::string id;
    std::string dir;
    std::string role;
    std::string state;
    std::string base;
    std::string minecraft;
    std::string core;
    bool managed = false;
    bool external = false;
    bool server = false;
    bool loader = false;
    bool complete = false;
    bool has_json = false;
    bool has_jar = false;
    bool has_natives = false;
};

struct CoreState {
    bool exists = false;
    bool managed = false;
    bool has_json = false;
    bool has_jar = false;
    bool has_natives = false;
};

enum class Kind { NewCore, Move, Drop, DropTree, Link, Marker };

struct Action {
    Kind kind = Kind::Move;
    std::string a;
    std::string b;
    std::string core;
    std::string entry;
    std::string minecraft;
    std::string old_core;
    std::string label;
};

struct Plan {
    std::vector<Action> actions;
    std::set<std::string> created;
    std::map<std::string, std::string> remap;  // 入口 id -> 实际核心 id
    std::vector<std::string> lines;
    int moved = 0;
    int linked = 0;
    int dropped = 0;
    int markers = 0;
    int skipped = 0;
    int cores = 0;

    void Line(const std::string& text);
    void NewCore(const std::string& core, const std::string& minecraft);
    void Move(const std::string& from, const std::string& to, const std::string& label);
    void Link(const std::string& from, const std::string& to, const std::string& label);
    void Drop(const std::string& path, const std::string& source, const std::string& label);
    void DropTree(const std::string& path, const std::string& source, const std::string& label);
    void Marker(const Entry& entry, const std::string& target);
    void Skip(const std::string& text);
};

bool LoadEntry(const Directory& paths, const std::string& id, Entry& entry);
void PlanVanilla(const Entry& entry, const Directory& paths, Plan& plan);
void PlanLoader(const Entry& entry, const Directory& paths,
                const std::map<std::string, Entry>& entries, Plan& plan);
void PlanServer(const Entry& entry, const Directory& paths,
                const std::map<std::string, Entry>& entries, Plan& plan);

}  // namespace migration
}  // namespace directory
