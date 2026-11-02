#include "app/download/Installer/Transfer/Transfer.h"

#include <filesystem>

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
    int done = 0;

    for (const auto& task : tasks) {
        ++done;
        if (progress) progress(task.label, done, total);

        if (Verify(task.dest, task.sha1)) {
            ++result.skipped;
            continue;
        }

        const size_t slash = task.dest.find_last_of('/');
        if (slash != std::string::npos) {
            if (!tools::EnsureDir(task.dest.substr(0, slash), error)) {
                ++result.failed;
                return false;
            }
        }

        const std::string part = task.dest + ".part";
        std::string download_error;
        if (!tools::DownloadFile(task.url, part, download_error)) {
            std::error_code ec;
            std::filesystem::remove(std::filesystem::u8path(part), ec);
            ++result.failed;
            error = task.label + ": " + download_error;
            return false;
        }

        std::string actual;
        if (!tools::Sha1File(part, actual) || tools::ToLower(actual) != tools::ToLower(task.sha1)) {
            std::error_code ec;
            std::filesystem::remove(std::filesystem::u8path(part), ec);
            ++result.failed;
            error = task.label + ": 校验失败";
            return false;
        }

        std::error_code ec;
        std::filesystem::rename(std::filesystem::u8path(part), std::filesystem::u8path(task.dest), ec);
        if (ec) {
            ++result.failed;
            error = task.label + ": 落盘失败";
            return false;
        }
        ++result.downloaded;
    }
    return true;
}

}  // namespace installer
}  // namespace download
