// 核心迁移的 WAL：每条文件操作先落日志再执行；回滚只反向执行日志里的记录。
#pragma once

#include <string>

namespace directory {
namespace migration {

class Journal {
public:
    // cores/.migration/<时间戳>/；目录名按字典序即时间序。
    bool Begin(const std::string& cores_root, std::string& error);
    const std::string& Dir() const { return dir_; }

    // 记录一行：kind \t a \t b；执行前调用。
    bool Record(const std::string& kind, const std::string& a, const std::string& b,
                std::string& error);
    bool WriteState(const std::string& state, std::string& error) const;

    // 回滚最近一次 committed 的迁移；成功时日志状态改成 rolledback。
    static bool RollbackLast(const std::string& game_root, const std::string& cores_root,
                             std::string& report, std::string& error);
    // 回滚指定日志目录（迁移中途失败时用）。
    static bool RollbackRun(const std::string& game_root, const std::string& run_dir,
                            std::string& report, std::string& error);

    static std::string BackupDir(const std::string& run_dir);

private:
    std::string dir_;
};

}  // namespace migration
}  // namespace directory
