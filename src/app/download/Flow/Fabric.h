// Fabric：meta 直接给一份可用的版本 json（含 inheritsFrom），落盘后按普通版本补全。
#pragma once

#include "app/download/Flow/Flow.h"

namespace download {
namespace flow {

class Fabric : public Flow<Fabric> {
public:
    bool RunImpl(const Context& context, const Request& request, const Progress& progress,
                 Outcome& outcome, std::string& error);
};

}  // namespace flow
}  // namespace download
