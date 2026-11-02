// 安装器：依赖版本源拿到的清单，负责把东西拿下来并放对位置。
#pragma once

#include "app/download/Installer/Placement/Placement.h"
#include "app/download/Installer/Transfer/Transfer.h"
#include "app/download/Installer/Version/Version.h"
#include "app/launch/Version/Version.h"

namespace download {

class Installer {
public:
    explicit Installer(const launch::Version& version) : version_(version) {}

    installer::Transfer& Transfers() { return transfer_; }
    installer::Version& Versions() { return version_; }
    installer::Placement& Placements() { return placement_; }

private:
    installer::Transfer transfer_;
    installer::Version version_;
    installer::Placement placement_;
};

}  // namespace download
