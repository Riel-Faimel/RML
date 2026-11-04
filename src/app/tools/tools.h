// 唯一允许的自由函数集合。
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace tools {

std::string ReadTextFile(const std::string& path);
bool WriteTextFile(const std::string& path, std::string_view text);
// 统一把 UTF-8 字符串转成文件系统路径（避免 u8path 在 C++20 被弃用）
std::filesystem::path ToPath(const std::string& text);

bool PathExists(const std::string& path);
bool IsDirectory(const std::string& path);
bool EnsureDir(const std::string& path, std::string& error);
std::string JoinPath(const std::string& left, const std::string& right);
std::string ToLower(std::string_view text);
bool EndsWith(std::string_view text, std::string_view suffix);

// maven 坐标（group:artifact:version[:classifier][@ext]）→ libraries 下的相对路径
std::string MavenToPath(std::string_view name);
std::vector<std::string> ListDirectories(const std::string& path);
std::vector<std::string> ListFiles(const std::string& path);

// 下载到文件（WinHTTP，跟随重定向）。同步阻塞，调用方放线程里。
bool DownloadFile(const std::string& url, const std::string& path, std::string& error);
// 下载过程中可查询取消；用于任务模块在文件下载途中取消。
bool DownloadFile(const std::string& url, const std::string& path, std::string& error,
                  const std::function<bool()>& cancelled);

// 取文本（清单、API 响应）。内部走 DownloadFile，少一份重复的 WinHTTP 逻辑。
bool HttpGet(const std::string& url, std::string& out, std::string& error);

// 带请求头的取文本（CurseForge 需要 x-api-key）
bool HttpGet(const std::string& url, const std::vector<std::string>& headers, std::string& out,
             std::string& error);

// 启动一个进程并等它结束；退出码 0 视为成功。
bool RunAndWait(const std::vector<std::string>& args, std::string& error);

// 同上，但把 stdout/stderr 重定向到 log_path（父进程不弹控制台，也拿不到屏幕输出）
bool RunAndWait(const std::vector<std::string>& args, const std::string& log_path, std::string& error);

// 睡一会儿（避免把 std::thread::sleep_for 拖进 winpthread 的 nanosleep64 依赖）
void SleepMs(int milliseconds);

// 解压（zip/tar.gz 都行，走系统自带的 bsdtar）
bool ExtractArchive(const std::string& archive, const std::string& dest, std::string& error);

// 只解一个条目（Forge/NeoForge 安装器 jar 里的 install_profile.json、version.json、
// maven/ 下的库都靠它取）。out_path 是解出来的完整路径。
bool ExtractEntry(const std::string& archive, const std::string& entry, const std::string& dest,
                  std::string& out_path, std::string& error);

// %APPDATA%\RML：RML 自己的数据根目录由 Directory 模块管理
// 校验与下载
std::string Sha1Hex(const void* data, size_t size);
bool Sha1File(const std::string& path, std::string& hex);

}  // namespace tools
