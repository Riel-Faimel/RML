// 原版：只把 Mojang 的版本描述落盘，剩下的交给安装器补全。
#pragma once

#include "app/directory/Flow/Flow.h"

namespace directory {
namespace flow {

class Vanilla : public Flow<Vanilla> {
public:
    bool RunImpl(const Context& context, const Request& request, const Progress& progress,
                 Outcome& outcome, std::string& error);
};

}  // namespace flow
}  // namespace directory
