// 模块 Download：全局命名空间中的类，组合版本源与安装器。
#pragma once

#include "app/directory/Installer/Installer.h"
#include "app/download/VersionSource/VersionSource.h"

class Download {
public:
    explicit Download(const launch::Version& version) : installer_(version) {}

    download::VersionSource& Sources() { return sources_; }
    directory::Installer& Installers() { return installer_; }

private:
    download::VersionSource sources_;
    directory::Installer installer_;
};
