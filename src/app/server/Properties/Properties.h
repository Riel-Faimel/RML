// 服务端配置：server.properties 的生成、解析与回写。
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "app/server/Instance/Instance.h"

namespace server {
namespace properties {

class File {
public:
    static std::string Path(const std::string& game_dir, const Instance& instance);

    // preserve_existing=false 按固定顺序重写；true 保留未知行，受管键按实例配置覆盖
    static std::string Build(const Instance& instance, const std::string& existing,
                             bool preserve_existing);

    static bool Write(const std::string& game_dir, const Instance& instance, bool preserve_existing,
                      std::string& error);

    // 解析成有序键值对；注释和空行跳过，同名键保留第一次出现
    static bool Read(const std::string& game_dir, const Instance& instance,
                     std::vector<std::pair<std::string, std::string>>& out, std::string& error);

    // 校验并写回全部条目，同时把受管键同步到 updated；catalog 由调用方保存
    static bool Apply(const std::string& game_dir, const Instance& current,
                      const std::vector<std::pair<std::string, std::string>>& entries,
                      Instance& updated, std::string& error);

    // 确保 server.properties、eula.txt 与空名单文件存在
    static bool Ensure(const std::string& game_dir, const Instance& instance, std::string& error);
};

}  // namespace properties
}  // namespace server
