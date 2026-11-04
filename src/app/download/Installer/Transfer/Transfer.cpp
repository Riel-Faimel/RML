#include "app/network/Network.h"
#include "app/download/Installer/Transfer/Transfer.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "app/nettask/NetTask.h"
#include "app/tools/tools.h"

namespace download {
namespace installer {

namespace {

bool EndsWithJar(const std::string& path) {
    if (path.size() < 4) return false;
    std::string suffix = path.substr(path.size() - 4);
    for (char& c : suffix) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return suffix == ".jar";
}

// 没有 sha1 的库（Fabric loader/intermediary）只能靠 ZIP 结构判断是不是下坏了
bool IsCompleteZip(const std::string& path) {
    std::ifstream stream(tools::ToPath(path), std::ios::binary);
    if (!stream) return false;
    stream.seekg(0, std::ios::end);
    const std::streamoff size = stream.tellg();
    if (size < 22) return false;
    const std::streamoff window = size > 65557 ? 65557 : size;
    stream.seekg(size - window, std::ios::beg);
    std::string tail(static_cast<size_t>(window), '\0');
    stream.read(tail.data(), static_cast<std::streamsize>(tail.size()));
    return tail.find("PK\x05\x06") != std::string::npos;
}

}  // namespace

bool Transfer::Verify(const std::string& path, const std::string& sha1) {
    if (!tools::PathExists(path)) return false;
    if (sha1.empty()) return true;

    std::string actual;
    if (!tools::Sha1File(path, actual)) return false;
    return tools::ToLower(actual) == tools::ToLower(sha1);
}

bool Transfer::Run(const std::vector<Task>& tasks, const Progress& progress, Result& result,
                   std::string& error) const {
    // 版本 json 里同一个库可能出现多次（例如 1.16.5 的 lwjgl 带不同 rules）。
    // 同一目标只能有一个下载任务，否则多个 worker 会同时写同一个 .part，互相覆盖后校验/落盘失败。
    std::vector<Task> pending;
    pending.reserve(tasks.size());
    std::unordered_map<std::string, size_t> by_dest;
    for (const Task& task : tasks) {
        if (task.dest.empty()) continue;
        const auto found = by_dest.find(task.dest);
        if (found == by_dest.end()) {
            by_dest.emplace(task.dest, pending.size());
            pending.push_back(task);
        } else if (pending[found->second].sha1.empty() && !task.sha1.empty()) {
            pending[found->second] = task;
        }
    }

    const int total = static_cast<int>(pending.size());
    if (total == 0) return true;

    if (task_manager_ != nullptr) {
        std::vector<NetTask::Resource> resources;
        resources.reserve(pending.size());
        for (const Task& task : pending) {
            NetTask::Resource resource;
            resource.url = task.url;
            resource.sha1 = task.sha1;
            resource.dest = task.dest;
            resource.label = task.label;
            resource.size = task.size;
            resources.push_back(std::move(resource));
        }
        std::string run_error;
        if (!task_manager_->Run("下载游戏文件", std::move(resources), run_error)) {
            error = run_error;
            return false;
        }
        result.downloaded = total;
        if (progress) progress("下载完成", total, total);
        return true;
    }

    auto cancelled = [this] { return cancel_.load(std::memory_order_relaxed); };

    std::atomic<size_t> next{0};
    std::atomic<int> done{0};
    std::atomic<int> downloaded{0};
    std::atomic<int> skipped{0};
    std::atomic<int> failed{0};
    std::atomic<bool> aborted{false};
    std::mutex error_mutex;
    std::string first_error;

    auto worker = [&]() {
        while (!aborted.load(std::memory_order_relaxed)) {
            if (cancelled()) {
                aborted = true;
                return;
            }
            const size_t index = next.fetch_add(1, std::memory_order_relaxed);
            if (index >= pending.size()) return;

            const Task& task = pending[index];
            bool valid = Verify(task.dest, task.sha1);
            if (valid && task.size > 0) {
                std::error_code size_error;
                const auto size = std::filesystem::file_size(tools::ToPath(task.dest), size_error);
                if (size_error || static_cast<int64_t>(size) != task.size) valid = false;
            }
            if (valid && task.sha1.empty() && EndsWithJar(task.dest)) {
                valid = IsCompleteZip(task.dest);
            }

            if (valid) {
                ++skipped;
            } else {
                std::string local_error;
                bool ok = true;

                const size_t slash = task.dest.find_last_of('/');
                if (slash != std::string::npos) {
                    ok = tools::EnsureDir(task.dest.substr(0, slash), local_error);
                }

                const std::string part = task.dest + ".part";
                if (ok) {
                    ok = false;
                    // 网络抖动或落盘被占用都重试，别为一次失败打断整个安装
                    for (int attempt = 0; attempt < 3; ++attempt) {
                        std::error_code ignored;
                        std::filesystem::remove(tools::ToPath(part), ignored);

                        if (!Network::download(task.url, part, local_error)) {
                            local_error = "下载失败: " + local_error;
                            tools::SleepMs(300);
                            continue;
                        }
                        if (cancelled()) {
                            local_error = "已取消";
                            break;
                        }

                        if (!task.sha1.empty()) {
                            std::string actual;
                            if (!tools::Sha1File(part, actual) ||
                                tools::ToLower(actual) != tools::ToLower(task.sha1)) {
                                local_error = "校验失败";
                                tools::SleepMs(300);
                                continue;
                            }
                        }
                        if (cancelled()) {
                            local_error = "已取消";
                            break;
                        }
                        if (task.sha1.empty() && EndsWithJar(task.dest) && !IsCompleteZip(part)) {
                            local_error = "文件不完整";
                            tools::SleepMs(300);
                            continue;
                        }

                        std::error_code ec;
                        std::filesystem::rename(tools::ToPath(part), tools::ToPath(task.dest), ec);
                        if (ec) {
                            // 目标已存在或被占用：先删目标再试一次
                            std::error_code remove_error;
                            std::filesystem::remove(tools::ToPath(task.dest), remove_error);
                            ec.clear();
                            std::filesystem::rename(tools::ToPath(part), tools::ToPath(task.dest),
                                                    ec);
                        }
                        if (!ec) {
                            ok = true;
                            break;
                        }
                        local_error = "落盘失败: " + task.dest;
                        tools::SleepMs(300);
                    }
                }

                if (ok) {
                    ++downloaded;
                } else {
                    std::error_code ec;
                    std::filesystem::remove(tools::ToPath(part), ec);
                    ++failed;
                    aborted = true;
                    std::lock_guard<std::mutex> lock(error_mutex);
                    if (first_error.empty()) first_error = task.label + ": " + local_error;
                }
            }

            const int current = ++done;
            if (progress) progress(task.label, current, total);
        }
    };

    // 8 路并发：assets 这类小文件吃延迟，串行会慢得离谱
    constexpr int kWorkers = 8;
    std::vector<std::thread> workers;
    workers.reserve(kWorkers);
    for (int i = 0; i < kWorkers; ++i) workers.emplace_back(worker);
    for (auto& worker_thread : workers) worker_thread.join();

    // 累加：安装器分两段（核心文件 → 资源）各调一次，计数要合并
    result.downloaded += downloaded.load();
    result.skipped += skipped.load();
    result.failed += failed.load();
    if (cancelled()) {
        error = "已取消";
        return false;
    }
    error = first_error;
    return failed.load() == 0;
}

}  // namespace installer
}  // namespace download
