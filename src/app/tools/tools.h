// 唯一允许的自由函数集合。
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tools {

std::string ReadTextFile(const std::string& path);
bool WriteTextFile(const std::string& path, std::string_view text);
bool PathExists(const std::string& path);
bool IsDirectory(const std::string& path);
bool EnsureDir(const std::string& path, std::string& error);
std::string JoinPath(const std::string& left, const std::string& right);
std::string ToLower(std::string_view text);
bool EndsWith(std::string_view text, std::string_view suffix);
std::vector<std::string> ListDirectories(const std::string& path);
std::vector<std::string> ListFiles(const std::string& path);

// 下载到文件（WinHTTP，跟随重定向）。同步阻塞，调用方放线程里。
bool DownloadFile(const std::string& url, const std::string& path, std::string& error);

// 取文本（清单、API 响应）。内部走 DownloadFile，少一份重复的 WinHTTP 逻辑。
bool HttpGet(const std::string& url, std::string& out, std::string& error);

// 带请求头的取文本（CurseForge 需要 x-api-key）
bool HttpGet(const std::string& url, const std::vector<std::string>& headers, std::string& out,
             std::string& error);

// 启动一个进程并等它结束；退出码 0 视为成功。
bool RunAndWait(const std::vector<std::string>& args, std::string& error);

// %APPDATA%\RML：RML 自己的数据根目录
std::string DataRoot();

// 校验与下载
std::string Sha1Hex(const void* data, size_t size);
bool Sha1File(const std::string& path, std::string& hex);

}  // namespace tools
