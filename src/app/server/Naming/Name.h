// 实例目录命名：把显示名变成安全的目录名，并避让已有目录。
#pragma once

#include <string>

#include "app/server/Catalog/Catalog.h"

namespace server {
namespace naming {

class Name {
public:
    // 保留中文/字母数字/._-，其它换成 _；避开 Windows 保留名
    static std::string Sanitize(const std::string& name);

    // 在 <gameDir>/servers 下取一个不冲突的目录名；keep_dir 是重命名时的原目录
    static std::string Unique(const std::string& name, const std::string& game_dir,
                              const std::string& keep_dir, const Catalog& catalog);
};

}  // namespace naming
}  // namespace server
