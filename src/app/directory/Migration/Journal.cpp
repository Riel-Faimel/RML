#include "app/directory/Migration/Journal.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <system_error>
#include <vector>

#include "app/json/JsonArena.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace directory {
namespace migration {

namespace {

std::filesystem::path P(const std::string& text) { return tools::ToPath(text); }

std::string Text(const std::filesystem::path& path) {
    const auto u8 = path.generic_u8string();
    return std::string(u8.begin(), u8.end());
}

std::string Clean(const std::string& field) {
    std::string out = field;
    for (char& c : out) {
        if (c == '\t' || c == '\r' || c == '\n') c = ' ';
    }
    return out;
}

std::string Absolute(const std::string& path) {
    std::error_code ec;
    auto absolute = std::filesystem::absolute(P(path), ec);
    return ec ? path : Text(absolute);
}

bool Inside(const std::string& path, const std::string& root) {
    const std::string full = Absolute(path);
    std::string prefix = Absolute(root);
    if (!prefix.empty() && prefix.back() != '/' && prefix.back() != '\\') prefix += '/';
    return full.size() >= prefix.size() &&
           tools::ToLower(full.substr(0, prefix.size())) == tools::ToLower(prefix);
}

bool EnsureParent(const std::string& path, std::string& error) {
    const size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return true;
    return tools::EnsureDir(path.substr(0, slash), error);
}

bool LinkOrCopy(const std::string& source, const std::string& dest, std::string& error) {
    if (!EnsureParent(dest, error)) return false;
    std::filesystem::remove(P(dest));
    std::error_code ec;
    std::filesystem::create_hard_link(P(source), P(dest), ec);
    if (ec) {
        ec.clear();
        std::filesystem::copy_file(P(source), P(dest),
                                   std::filesystem::copy_options::overwrite_existing, ec);
    }
    if (ec) {
        error = "回滚时链接/复制失败: " + source + " -> " + dest;
        return false;
    }
    return true;
}

bool RestoreTree(const std::string& source, const std::string& dest, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::is_directory(P(source), ec)) return true;
    if (!EnsureParent(dest, error)) return false;
    std::filesystem::create_directories(P(dest), ec);
    if (ec) {
        error = "回滚时创建目录失败: " + dest;
        return false;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(P(source), ec)) {
        if (ec) continue;
        const auto relative = std::filesystem::relative(entry.path(), P(source), ec);
        if (ec) continue;
        const std::string target = tools::JoinPath(dest, Text(relative));
        if (entry.is_directory()) {
            std::filesystem::create_directories(P(target), ec);
        } else if (entry.is_regular_file()) {
            if (!LinkOrCopy(Text(entry.path()), target, error)) return false;
        }
    }
    if (ec) {
        error = "回滚遍历目录失败: " + source;
        return false;
    }
    return true;
}

bool ReadState(const std::string& run_dir, std::string& state) {
    state = tools::ReadTextFile(run_dir + "/state.txt");
    const size_t space = state.find_first_of(" \r\n");
    if (space != std::string::npos) state = state.substr(0, space);
    return !state.empty();
}

void AddLine(std::string& report, const std::string& line) {
    if (!report.empty()) report += "\n";
    report += line;
}

}  // namespace

std::string Journal::BackupDir(const std::string& run_dir) { return run_dir + "/backup"; }

bool Journal::Begin(const std::string& cores_root, std::string& error) {
    const std::string root = tools::JoinPath(cores_root, ".migration");
    if (!tools::EnsureDir(root, error)) return false;

    std::string stamp;
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto now = std::chrono::system_clock::now();
        const std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &time);
#else
        localtime_r(&time, &local);
#endif
        char buffer[64] = {};
        std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d-%02d%02d%02d-%03d", local.tm_year + 1900,
                      local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec,
                      static_cast<int>(
                          std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
                              .count() %
                          1000));
        const std::string candidate = tools::JoinPath(root, std::string(buffer));
        if (!tools::PathExists(candidate)) {
            if (!tools::EnsureDir(candidate, error)) return false;
            dir_ = candidate;
            return true;
        }
        tools::SleepMs(2);
    }
    error = "无法创建迁移日志目录: " + root;
    return false;
}

bool Journal::Record(const std::string& kind, const std::string& a, const std::string& b,
                     std::string& error) {
    if (dir_.empty()) {
        error = "迁移日志尚未开始";
        return false;
    }
    std::ofstream out(P(dir_ + "/journal.log"), std::ios::binary | std::ios::app);
    if (!out) {
        error = "写迁移日志失败: " + dir_;
        return false;
    }
    out << kind << '\t' << Clean(a) << '\t' << Clean(b) << '\n';
    out.flush();
    if (!out) {
        error = "写迁移日志失败: " + dir_;
        return false;
    }
    return true;
}

bool Journal::WriteState(const std::string& state, std::string& error) const {
    if (dir_.empty()) {
        error = "迁移日志目录为空";
        return false;
    }
    if (!tools::WriteTextFile(dir_ + "/state.txt", state + "\n")) {
        error = "写迁移状态失败: " + dir_;
        return false;
    }
    return true;
}

bool Journal::RollbackLast(const std::string& game_root, const std::string& cores_root,
                           std::string& report, std::string& error) {
    const std::string root = tools::JoinPath(cores_root, ".migration");
    std::vector<std::string> runs = tools::ListDirectories(root);
    std::sort(runs.begin(), runs.end(), std::greater<std::string>());
    for (const std::string& name : runs) {
        const std::string run_dir = tools::JoinPath(root, name);
        std::string state;
        if (!ReadState(run_dir, state) || (state != "committed" && state != "failed")) continue;
        return RollbackRun(game_root, run_dir, report, error);
    }
    error = "没有可回滚的核心迁移记录";
    return false;
}

bool Journal::RollbackRun(const std::string& game_root, const std::string& run_dir,
                          std::string& report, std::string& error) {
    const std::string log_path = run_dir + "/journal.log";
    const std::string text = tools::ReadTextFile(log_path);
    if (text.empty()) {
        error = "迁移日志为空: " + log_path;
        return false;
    }

    struct Row {
        std::string kind;
        std::string a;
        std::string b;
    };
    std::vector<Row> rows;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(start, end - start);
        start = end + 1;
        if (line.empty()) continue;
        const size_t first = line.find('\t');
        const size_t second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
        if (second == std::string::npos) continue;
        rows.push_back({line.substr(0, first), line.substr(first + 1, second - first - 1),
                        line.substr(second + 1)});
    }

    for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
        const Row& row = *it;
        // marker 的 b 字段是旧 core id，不是路径；只有文件类记录才校验两侧路径。
        const bool path_row = row.kind == "move" || row.kind == "drop" || row.kind == "dropdir" ||
                              row.kind == "link" || row.kind == "newcore";
        if (path_row) {
            for (const std::string* path : {&row.a, &row.b}) {
                if (path->empty()) continue;
                if (!Inside(*path, game_root)) {
                    error = "迁移日志路径越界，拒绝回滚: " + *path;
                    return false;
                }
            }
        } else if (row.kind == "marker" && !Inside(row.a, game_root)) {
            error = "迁移日志路径越界，拒绝回滚: " + row.a;
            return false;
        }

        if (row.kind == "marker") {
            if (!tools::PathExists(row.a)) continue;
            auto j = JsonArena::Instance().Open(row.a);
            if (!j) {
                error = j.error();
                return false;
            }
            if (row.b.empty() || row.b == "-") {
                j.remove("core");
            } else {
                j["core"] = Json::Value::String(row.b);
            }
            std::string commit_error;
            if (!j.commit(commit_error)) {
                error = commit_error;
                return false;
            }
            AddLine(report, "恢复入口 core: " + row.a);
            continue;
        }

        if (row.kind == "newcore") {
            std::error_code ec;
            std::filesystem::remove_all(P(row.a), ec);
            if (ec) {
                error = "回滚时删除新核心失败: " + row.a;
                return false;
            }
            AddLine(report, "删除本次新建核心: " + row.a);
            continue;
        }

        if (row.kind == "move") {
            if (!tools::PathExists(row.b)) continue;
            if (tools::PathExists(row.a)) {
                error = "回滚冲突，目标位置已有文件: " + row.a;
                return false;
            }
            if (!EnsureParent(row.a, error)) return false;
            std::error_code ec;
            std::filesystem::rename(P(row.b), P(row.a), ec);
            if (ec) {
                ec.clear();
                if (!LinkOrCopy(row.b, row.a, error)) return false;
                std::error_code ignored;
                std::filesystem::remove(P(row.b), ignored);
            }
            if (!tools::PathExists(row.a)) {
                error = "回滚移动失败: " + row.b + " -> " + row.a;
                return false;
            }
            AddLine(report, "移回: " + row.b + " -> " + row.a);
            continue;
        }

        if (row.kind == "drop") {
            if (tools::PathExists(row.a)) continue;
            if (!tools::PathExists(row.b)) {
                error = "回滚时找不到源文件: " + row.b;
                return false;
            }
            if (!LinkOrCopy(row.b, row.a, error)) return false;
            AddLine(report, "恢复入口文件: " + row.a);
            continue;
        }

        if (row.kind == "dropdir") {
            if (tools::PathExists(row.a)) continue;
            if (!RestoreTree(row.b, row.a, error)) return false;
            AddLine(report, "恢复入口目录: " + row.a);
            continue;
        }

        if (row.kind == "link") {
            std::error_code ec;
            std::filesystem::remove_all(P(row.a), ec);
            AddLine(report, "删除复制到核心的文件: " + row.a);
        }
    }

    std::string state_error;
    Journal journal;
    journal.dir_ = run_dir;
    if (!journal.WriteState("rolledback", state_error)) {
        error = state_error;
        return false;
    }
    return true;
}

}  // namespace migration
}  // namespace directory
