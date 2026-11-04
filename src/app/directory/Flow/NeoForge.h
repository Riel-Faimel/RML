// NeoForge：版本号从 maven metadata 里挑，安装流程和 Forge 一样走 install_profile。
#pragma once

#include "app/directory/Flow/Flow.h"

namespace directory {
namespace flow {

class NeoForge : public Flow<NeoForge> {
public:
    bool RunImpl(const Context& context, const Request& request, const Progress& progress,
                 Outcome& outcome, std::string& error);
};

}  // namespace flow
}  // namespace directory
