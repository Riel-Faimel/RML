#include "app/download/Installer/Transfer/Transfer.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>

#include "app/tools/tools.h"

namespace download {
namespace installer {

bool Transfer::Verify(const std::string& path, const std::string& sha1) {
    if (!tools::PathExists(path)) return false;
    if (sha1.empty()) return true;

    std::string actual;
    if (!tools::Sha1File(path, actual)) return false;
    return tools::ToLower(actual) == tools::ToLower(sha1);
}

bool Transfer::Run(const std::vector<Task>& tasks, const Progress& progress, Result& result,
                   std::string& error) const {
    const int total = static_cast<int>(tasks.size());
    if (total == 0) return true;

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
            const size_t index = next.fetch_add(1, std::memory_order_relaxed);
            if (index >= tasks.size()) return;

            const Task& task = tasks[index];
            if (Verify(task.dest, task.sha1)) {
                ++skipped;
            } else {
                std::string local_error;
                bool ok = true;

                const size_t slash = task.dest.find_last_of('/');
                if (slash != std::string::npos) {
                    ok = tools::EnsureDir(task.dest.substr(0, slash), local_error);
                }

                const std::string part = task.dest + ".part";
                if (ok && !tools::DownloadFile(task.url, part, local_error)) ok = false;

                if (ok) {
                    std::string actual;
                    if (!tools::Sha1File(part, actual) ||
                        tools::ToLower(actual) != tools::ToLower(task.sha1)) {
                        ok = false;
                        local_error = "校验失败";
                    }
                }

                if (ok) {
                    std::error_code ec;
                    std::filesystem::rename(std::filesystem::u8path(part),
                                            std::filesystem::u8path(task.dest), ec);
                    if (ec) {
                        ok = false;
                        local_error = "落盘失败";
                    }
                }

                if (ok) {
                    ++downloaded;
                } else {
                    std::error_code ec;
                    std::filesystem::remove(std::filesystem::u8path(part), ec);
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
    error = first_error;
    return failed.load() == 0;
}

}  // namespace installer
}  // namespace download
