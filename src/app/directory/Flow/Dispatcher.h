// 加载器分发：按名字挑一个 Flow 子类。这里是唯一认「原版 / Fabric / Forge / NeoForge」
// 的地方，策略本身不写加载器分支。
#pragma once

#include "app/directory/Flow/Flow.h"

namespace directory {
namespace flow {

class Dispatcher {
public:
    bool Run(const Context& context, const Request& request, const Progress& progress,
             Outcome& outcome, std::string& error) const;
};

}  // namespace flow
}  // namespace directory
