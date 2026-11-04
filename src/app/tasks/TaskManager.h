// 任务模块：只负责“从网络下载文件”的过程，不负责安装。
// 其他模块提交资源列表，由这里排队、下载、暂停/继续/取消。
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

class TaskManager {
public:
    using Id = uint64_t;

    struct Resource {
        std::string url;
        std::string sha1;
        std::string dest;
        std::string label;
        int64_t size = 0;
    };

    struct Snapshot {
        Id id = 0;
        std::string title;
        std::string stage;
        std::string status;  // queued/running/paused/completed/failed/cancelled
        std::string error;
        double progress = 0.0;
        int64_t bytes = 0;
        int64_t total = 0;
    };

    using Done = std::function<void(bool ok, const std::string& error)>;

    TaskManager();
    ~TaskManager();

    Id Submit(std::string title, std::vector<Resource> resources, Done done = {});

    // 安装流程使用：提交后等待下载完成，但安装本身不算任务。
    bool Run(const std::string& title, std::vector<Resource> resources, std::string& error);
    void Pause(Id id);
    void Resume(Id id);
    void Cancel(Id id);

    std::vector<Snapshot> Snapshots() const;

private:
    struct Task {
        Id id = 0;
        std::string title;
        std::string stage;
        std::string status;
        std::string error;
        double progress = 0.0;
        int64_t bytes = 0;
        int64_t total = 0;
        bool paused = false;
        bool cancelled = false;
        std::vector<Resource> resources;
        Done done;
    };

    void Run();
    void WaitIfPaused(Task& task);
    bool DownloadOne(Task& task, const Resource& resource, std::string& error);

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<Id> queue_;
    std::deque<Task> tasks_;
    std::thread worker_;
    bool stopping_ = false;
    Id next_id_ = 1;
};
