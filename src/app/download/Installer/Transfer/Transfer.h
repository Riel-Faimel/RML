// 安装器的传输底座：只认 URL 和 sha1，不关心里面是 jar 还是资源。
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace download {
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

    // 单独校验一个文件，供“只检查不下载”用
    static bool Verify(const std::string& path, const std::string& sha1);
};

}  // namespace installer
}  // namespace download
