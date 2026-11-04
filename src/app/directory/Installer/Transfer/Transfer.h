// 安装器的传输底座：只认 URL 和 sha1，不关心里面是 jar 还是资源。
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class NetTask;

namespace directory {
namespace installer {

class Transfer {
public:
    struct Task {
        std::string url;
        std::string sha1;
        std::string dest;
        std::string label;
        int64_t size = 0;
    };

    struct Result {
        int downloaded = 0;
        int skipped = 0;
        int failed = 0;
    };

    using Progress = std::function<void(const std::string& label, int done, int total)>;

    // 已有且校验通过的文件会被跳过；失败即中断并返回原因。
    bool Run(const std::vector<Task>& tasks, const Progress& progress, Result& result,
             std::string& error) const;

    // 启动页“取消自动补齐”用；当前文件下载完后停止，未开始的 worker 直接退出。
    void Cancel() const { cancel_.store(true, std::memory_order_relaxed); }
    void ResetCancel() const { cancel_.store(false, std::memory_order_relaxed); }

    // 安装流程的下载批次交给任务模块；自动补全传 nullptr。
    void SetNetTask(NetTask* task_manager) { task_manager_ = task_manager; }
    NetTask* task_manager() const { return task_manager_; }

private:
    static bool Verify(const std::string& path, const std::string& sha1);

    mutable std::atomic<bool> cancel_{false};
    NetTask* task_manager_ = nullptr;
};

}  // namespace installer
}  // namespace directory
