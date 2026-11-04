// Forge：从 promotions 挑版本 → 走 install_profile 的处理器流程。
#pragma once

#include "app/directory/Flow/Flow.h"

namespace directory {
namespace flow {

class Forge : public Flow<Forge> {
public:
    bool RunImpl(const Context& context, const Request& request, const Progress& progress,
                 Outcome& outcome, std::string& error);
};

}  // namespace flow
}  // namespace directory
