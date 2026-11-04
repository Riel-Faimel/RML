#include "app/network/Network.h"
#include "app/nettask/NetTask.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <thread>
#include <unordered_set>

#include "app/tools/tools.h"

namespace {

// 落盘偶尔会被杀软/索引器短暂占用；rename 失败后等一下再试，别直接报失败。
bool CommitDownload(const std::string& part, const std::string& dest, std::string& error) {
    for (int attempt = 0; attempt < 10; ++attempt) {
        std::error_code ec;
        std::filesystem::rename(tools::ToPath(part), tools::ToPath(dest), ec);
        if (!ec) return true;
        error = ec.message();

        std::error_code ignored;
        std::filesystem::remove(tools::ToPath(dest), ignored);
        if (attempt < 9) tools::SleepMs(300);
    }

    // rename 被占用时退化成复制，至少把下载结果落到目标。
    std::error_code copy_error;
    std::filesystem::copy_file(tools::ToPath(part), tools::ToPath(dest),
                               std::filesystem::copy_options::overwrite_existing, copy_error);
    if (!copy_error) {
        std::error_code ignored;
        std::filesystem::remove(tools::ToPath(part), ignored);
        return true;
    }
    error = copy_error.message();
    return false;
}

}  // namespace

NetTask::NetTask() {
    // 任务之间并行；单任务内部再由 Execute 开一组下载线程。
    constexpr int kTaskWorkers = 4;
    for (int i = 0; i < kTaskWorkers; ++i) {
        workers_.emplace_back([this] { Worker(); });
    }
}

NetTask::~NetTask() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        for (auto& task : tasks_) task.cancelled = true;
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

NetTask::Id NetTask::Submit(std::string title, std::vector<Resource> resources, Done done) {
    std::lock_guard<std::mutex> lock(mutex_);
    Task task;
    task.id = next_id_++;
    task.title = std::move(title);
    task.status = "queued";
    task.stage = "等待下载";

    // 同一个目标只保留一份，避免并行下载时两个线程抢同一个 .part。
    std::unordered_set<std::string> seen;
    std::vector<Resource> unique;
    for (auto& resource : resources) {
        if (!resource.dest.empty() && !seen.insert(resource.dest).second) continue;
        unique.push_back(std::move(resource));
    }
    task.resources = std::move(unique);
    task.done = std::move(done);
    tasks_.push_back(std::move(task));
    queue_.push(tasks_.back().id);
    cv_.notify_one();
    return tasks_.back().id;
}

bool NetTask::Run(const std::string& title, std::vector<Resource> resources,
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

void NetTask::Pause(Id id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& task : tasks_) {
        if (task.id != id || task.status != "running") continue;
        task.paused = true;
        task.status = "paused";
        task.stage = "已暂停";
    }
}

void NetTask::Resume(Id id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& task : tasks_) {
        if (task.id != id || task.status != "paused") continue;
        task.paused = false;
        task.status = "running";
        task.stage = "继续下载";
    }
    cv_.notify_all();
}

void NetTask::Cancel(Id id) {
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

void NetTask::Retry(Id id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& task : tasks_) {
        if (task.id != id) continue;
        if (task.status != "failed" && task.status != "cancelled") return;
        task.status = "queued";
        task.stage = "等待重试";
        task.error.clear();
        task.paused = false;
        task.cancelled = false;
        queue_.push(id);
        cv_.notify_one();
        return;
    }
}

std::vector<NetTask::Snapshot> NetTask::Snapshots() const {
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
        out.push_back(std::move(snapshot));
    }
    return out;
}

void NetTask::WaitIfPaused(Task& task) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] { return !task.paused || task.cancelled || stopping_; });
}

void NetTask::Worker() {
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

        Execute(*task);
    }
}

void NetTask::Execute(Task& task) {
    const size_t total = task.resources.size();
    std::atomic<size_t> next{0};
    std::atomic<size_t> finished{0};
    std::atomic<size_t> failed{0};
    std::atomic<bool> aborted{false};
    std::mutex error_mutex;
    std::string first_error;

    auto download_worker = [&] {
        while (!aborted.load(std::memory_order_relaxed)) {
            const size_t index = next.fetch_add(1, std::memory_order_relaxed);
            if (index >= total) return;

            WaitIfPaused(task);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (task.cancelled || stopping_) {
                    aborted.store(true, std::memory_order_relaxed);
                    return;
                }
                task.stage = "下载 " + task.resources[index].label;
            }

            std::string error;
            if (!DownloadOne(task, task.resources[index], error)) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (task.cancelled || stopping_) {
                        aborted.store(true, std::memory_order_relaxed);
                        return;
                    }
                }
                // 单个文件失败不打断其余文件；全部跑完后再统一报失败。
                std::lock_guard<std::mutex> lock(error_mutex);
                if (first_error.empty()) first_error = error;
                ++failed;
                continue;
            }

            // 单文件结束后再检查一次暂停/取消，别让最后一个文件绕过暂停。
            WaitIfPaused(task);
            std::lock_guard<std::mutex> lock(mutex_);
            if (task.cancelled || stopping_) {
                aborted.store(true, std::memory_order_relaxed);
                return;
            }
            ++finished;
            task.progress = total > 0 ? static_cast<double>(finished.load()) / total : 1.0;
        }
    };

    if (total > 0) {
        const size_t count = std::min<size_t>(4, total);
        if (count <= 1) {
            download_worker();
        } else {
            std::vector<std::thread> pool;
            pool.reserve(count);
            for (size_t i = 0; i < count; ++i) pool.emplace_back(download_worker);
            for (auto& thread : pool) thread.join();
        }
    }

    const bool ok = !aborted.load() && failed.load() == 0 && finished.load() == total;
    Done done;
    std::string final_error;
    bool completed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (task.cancelled || stopping_) {
            task.status = "cancelled";
            task.stage = "已取消";
            if (task.progress < 1.0) task.error = "已取消";
            final_error = "已取消";
        } else if (ok) {
            task.status = "completed";
            task.stage = "下载完成";
            task.progress = 1.0;
            completed = true;
        } else {
            task.status = "failed";
            task.stage = "下载失败";
            task.error = first_error.empty() ? "下载失败" : first_error;
            final_error = task.error;
        }
        done = task.done;
    }
    cv_.notify_all();
    if (done) done(completed, final_error);
}

bool NetTask::DownloadOne(Task& task, const Resource& resource, std::string& error) {
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

    const std::string part = resource.dest + ".part." + std::to_string(task.id);
    bool downloaded = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (attempt > 0) std::this_thread::sleep_for(std::chrono::milliseconds(300));
        error.clear();
        downloaded = Network::download(resource.url, part, error, [this, &task] {
            std::lock_guard<std::mutex> lock(mutex_);
            return task.cancelled || stopping_;
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

    std::string rename_error;
    if (!CommitDownload(part, resource.dest, rename_error)) {
        error = resource.label + ": 落盘失败（" + rename_error + "）";
        std::error_code ignored;
        std::filesystem::remove(tools::ToPath(part), ignored);
        return false;
    }
    return true;
}
