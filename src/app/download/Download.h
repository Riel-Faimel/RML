// 模块 Download：全局命名空间中的类，组合版本源与安装器。
#pragma once

#include "app/download/Installer/Installer.h"
#include "app/download/VersionSource/VersionSource.h"

class Download {
public:
    explicit Download(const launch::Version& version) : installer_(version) {}

    download::VersionSource& Sources() { return sources_; }
    download::Installer& Installers() { return installer_; }

private:
    download::VersionSource sources_;
    download::Installer installer_;
};
