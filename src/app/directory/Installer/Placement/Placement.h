// 落盘规则：一份内容该放到实例的哪个子目录、重名怎么处理。
#pragma once

#include <string>

namespace directory {
namespace installer {

class Placement {
public:
    enum class Kind { Mod, Shaderpack, Resourcepack };

    // 实例目录下对应的子目录（不存在也没关系）
    static std::string Directory(const std::string& instance_dir, Kind kind);

    // 目标文件路径；overwrite 为 false 且已存在时自动加 (1)(2) 后缀
    static std::string Resolve(const std::string& instance_dir, Kind kind,
                               const std::string& filename, bool overwrite);
};

}  // namespace installer
}  // namespace directory
