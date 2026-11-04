// 服务端安装器：把 server.jar 下载进 versions/<id>，并在 rml.json 里标记 kind=server。
// 只做原版与 Fabric；其它加载器在调用方给出明确提示。
#pragma once

#include <string>

#include "app/directory/Installer/Transfer/Transfer.h"
#include "app/download/VersionSource/VersionSource.h"
#include "app/launch/Environment/Environment.h"
#include "app/launch/Version/Version.h"

namespace server {

class Installer {
public:
    using Progress = directory::installer::Transfer::Progress;

    Installer(launch::Version& versions, launch::Environment& environment,
              download::VersionSource& sources,
              directory::installer::Transfer& transfer);

    bool Vanilla(const std::string& mc_version, const Progress& progress, std::string& version_id,
                 std::string& error) const;
    bool Fabric(const std::string& mc_version, const Progress& progress, std::string& version_id,
                std::string& error) const;

private:
    bool Prepare(const std::string& id, const std::string& name, const std::string& loader,
                 const std::string& mc_version, int java_major, std::string& error) const;
    bool WriteMarker(const std::string& id, const std::string& name, const std::string& loader,
                     const std::string& mc_version, int java_major, std::string& error) const;

    launch::Version& versions_;
    launch::Environment& environment_;
    download::VersionSource& sources_;
    directory::installer::Transfer& transfer_;
};

}  // namespace server
