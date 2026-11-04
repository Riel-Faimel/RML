#include "app/server/Naming/Name.h"

#include <cctype>

#include "app/tools/tools.h"

namespace server {
namespace naming {

std::string Name::Sanitize(const std::string& name) {
    auto trimmable = [](unsigned char c) {
        return std::isspace(c) != 0 || c == '.';
    };

    size_t begin = 0;
    size_t end = name.size();
    while (begin < end && trimmable(static_cast<unsigned char>(name[begin]))) ++begin;
    while (end > begin && trimmable(static_cast<unsigned char>(name[end - 1]))) --end;

    std::string out;
    out.reserve(end - begin);
    for (size_t i = begin; i < end; ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (c >= 0x80 || std::isalnum(c) != 0 || c == '.' || c == '_' || c == '-') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('_');
        }
    }

    // Windows 不允许目录名以点或空格结尾
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
    while (!out.empty() && (out.front() == '.' || out.front() == ' ')) out.erase(out.begin());

    if (out.empty()) out = "server";

    static const char* const kReserved[] = {"con",  "prn",  "aux",  "nul",  "com1", "com2",
                                            "com3", "com4", "com5", "com6", "com7", "com8",
                                            "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5",
                                            "lpt6", "lpt7", "lpt8", "lpt9"};
    const std::string lowered = tools::ToLower(out);
    for (const char* reserved : kReserved) {
        if (lowered == reserved) {
            out += "_";
            break;
        }
    }
    return out;
}

std::string Name::Unique(const std::string& name, const std::string& game_dir,
                         const std::string& keep_dir, const Catalog& catalog) {
    const std::string servers = Catalog::ServersDir(game_dir);
    const std::string base = Sanitize(name);
    std::string dir = base;
    int suffix = 2;
    while (dir != keep_dir &&
           (catalog.IndexOf(dir) >= 0 || tools::PathExists(tools::JoinPath(servers, dir)))) {
        dir = base + "_" + std::to_string(suffix++);
    }
    return dir;
}

}  // namespace naming
}  // namespace server
