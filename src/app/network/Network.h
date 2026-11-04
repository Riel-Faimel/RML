#pragma once

#include <functional>
#include <string>
#include <vector>

enum class HttpCMD {
    GET,
};

class Network {
public:
    static bool http(HttpCMD command, const std::string& url, std::string& out,
                     std::string& error);
    static bool http(HttpCMD command, const std::string& url,
                     const std::vector<std::string>& headers, std::string& out,
                     std::string& error);
    static bool download(const std::string& url, const std::string& path, std::string& error);
    static bool download(const std::string& url, const std::string& path, std::string& error,
                         const std::function<bool()>& cancelled);
};
