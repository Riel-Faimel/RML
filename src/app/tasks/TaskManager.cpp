#include "app/tasks/TaskManager.h"

#include <chrono>
#include <filesystem>
#include <system_error>
#include <thread>

#include "app/tools/tools.h"

TaskManager::TaskManager() : worker_([this] { Run(); }) {}

TaskManager::~TaskManager() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        for (auto& task : tasks_) {
            if (task.status == "queued" || task.status == "paused") task.cancelled = true;
        }
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

TaskManager::Id TaskManager::Submit(std::string title, std::vector<Resource> resources, Done done) {
    std::lock_guard<std::mutex> lock(mutex_);
    Task task;
    task.id = next_id_++;
    task.title = std::move(title);
    task.status = "queued";
    task.stage = "等待下载";
    task.resources = std::move(resources);
    task.done = std::move(done);
    tasks_.push_back(std::move(task));
    queue_.push(tasks_.back().id);
    cv_.notify_one();
    return tasks_.back().id;
}

bool TaskManager::Run(const std::string& title, std::vector<Resource> resources,
                      std::string& error) {
    const Id id = Submit(title, std::move(resources), {});
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] {
        for (const auto& task : tasks_) {
            if (task.id != id) continue;
            return task.status == "completed" || task.status == "failed" ||
                   task.status == "cancelled";
        }
        return true;
    });
    for (const auto& task : tasks_) {
        if (task.id != id) continue;
        if (task.status == "completed") return true;
        error = task.error.empty() ? "下载已取消" : task.error;
        return false;
    }
    error = "下载任务不存在";
    return false;
}

void TaskManager::Pause(Id id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& task : tasks_) {
        if (task.id != id || task.status != "running") continue;
        task.paused = true;
        task.status = "paused";
        task.stage = "已暂停";
    }
}

void TaskManager::Resume(Id id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& task : tasks_) {
        if (task.id != id || task.status != "paused") continue;
        task.paused = false;
        task.status = "running";
        task.stage = "继续下载";
    }
    cv_.notify_all();
}

void TaskManager::Cancel(Id id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& task : tasks_) {
        if (task.id != id) continue;
        task.cancelled = true;
        if (task.status == "queued" || task.status == "paused") {
            task.status = "cancelled";
            task.stage = "已取消";
        }
    }
    cv_.notify_all();
}

std::vector<TaskManager::Snapshot> TaskManager::Snapshots() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Snapshot> out;
    out.reserve(tasks_.size());
    for (const auto& task : tasks_) {
        Snapshot snapshot;
        snapshot.id = task.id;
        snapshot.title = task.title;
        snapshot.stage = task.stage;
        snapshot.status = task.status;
        snapshot.error = task.error;
        snapshot.progress = task.progress;
        snapshot.bytes = task.bytes;
        snapshot.total = task.total;
        out.push_back(std::move(snapshot));
    }
    return out;
}

void TaskManager::WaitIfPaused(Task& task) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] { return !task.paused || task.cancelled || stopping_; });
}

void TaskManager::Run() {
    while (true) {
        Id id = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
            if (stopping_) return;
            id = queue_.front();
            queue_.pop();
            for (auto& task : tasks_) {
                if (task.id != id) continue;
                if (task.cancelled || task.status == "cancelled") {
                    task.status = "cancelled";
                    task.stage = "已取消";
                } else {
                    task.status = "running";
                    task.stage = "准备下载";
                }
                break;
            }
        }

        Task* task = nullptr;
        Done cancelled_done;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& item : tasks_) {
                if (item.id == id) {
                    if (item.status == "cancelled") {
                        cancelled_done = item.done;
                        task = nullptr;
                        break;
                    }
                    task = &item;
                    break;
                }
            }
        }
        if (task == nullptr) {
            if (cancelled_done) cancelled_done(false, "已取消");
            continue;
        }

        std::string error;
        bool ok = true;
        const int64_t total = static_cast<int64_t>(task->resources.size());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            task->total = total;
        }
        for (size_t i = 0; i < task->resources.size(); ++i) {
            WaitIfPaused(*task);
            if (task->cancelled) {
                ok = false;
                error = "已取消";
                break;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                task->stage = "下载 " + task->resources[i].label;
            }
            if (!DownloadOne(*task, task->resources[i], error)) {
                ok = false;
                break;
            }

            // 单文件下载结束后也检查一次暂停/取消；否则一个文件的 Mod 任务
            // 会在用户刚点暂停后马上被标成完成。
            WaitIfPaused(*task);
            if (task->cancelled) {
                ok = false;
                error = "已取消";
                break;
            }

            std::lock_guard<std::mutex> lock(mutex_);
            task->progress = total > 0 ? static_cast<double>(i + 1) / total : 1.0;
            task->bytes = static_cast<int64_t>(i + 1);
        }

        Done done;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (task->cancelled) {
                task->status = "cancelled";
                task->stage = "已取消";
                if (task->progress < 1.0) task->error = "已取消";
            } else if (ok) {
                task->status = "completed";
                task->stage = "下载完成";
                task->progress = 1.0;
            } else {
                task->status = "failed";
                task->stage = "下载失败";
                task->error = error;
            }
            done = task->done;
        }
        cv_.notify_all();
        if (done) done(ok, error);
    }
}

bool TaskManager::DownloadOne(Task& task, const Resource& resource, std::string& error) {
    if (resource.dest.empty()) {
        error = "下载目标为空: " + resource.label;
        return false;
    }

    if (tools::PathExists(resource.dest)) {
        if (!resource.sha1.empty()) {
            std::string actual;
            if (tools::Sha1File(resource.dest, actual) &&
                tools::ToLower(actual) == tools::ToLower(resource.sha1)) {
                return true;
            }
        } else if (resource.size > 0) {
            std::error_code size_error;
            const auto size = std::filesystem::file_size(tools::ToPath(resource.dest), size_error);
            if (!size_error && static_cast<int64_t>(size) == resource.size) return true;
        }
    }

    const size_t slash = resource.dest.find_last_of('/');
    if (slash != std::string::npos) {
        std::string dir_error;
        if (!tools::EnsureDir(resource.dest.substr(0, slash), dir_error)) {
            error = dir_error;
            return false;
        }
    }

    const std::string part = resource.dest + ".part";
    bool downloaded = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (attempt > 0) std::this_thread::sleep_for(std::chrono::milliseconds(300));
        error.clear();
        downloaded = tools::DownloadFile(resource.url, part, error, [this, &task] {
            std::lock_guard<std::mutex> lock(mutex_);
            return task.cancelled;
        });
        if (downloaded) break;
        if (error == "cancelled") break;
    }
    if (!downloaded) {
        error = resource.label + ": " + error;
        std::error_code ignored;
        std::filesystem::remove(tools::ToPath(part), ignored);
        return false;
    }

    if (!resource.sha1.empty()) {
        std::string actual;
        if (!tools::Sha1File(part, actual) ||
            tools::ToLower(actual) != tools::ToLower(resource.sha1)) {
            error = resource.label + ": 校验失败";
            std::error_code ignored;
            std::filesystem::remove(tools::ToPath(part), ignored);
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tools::ToPath(part), tools::ToPath(resource.dest), ec);
    if (ec) {
        std::filesystem::remove(tools::ToPath(resource.dest), ec);
        ec.clear();
        std::filesystem::rename(tools::ToPath(part), tools::ToPath(resource.dest), ec);
    }
    if (ec) {
        error = resource.label + ": 落盘失败";
        std::error_code ignored;
        std::filesystem::remove(tools::ToPath(part), ignored);
        return false;
    }
    return true;
}
