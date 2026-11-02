#include "app/download/Installer/Placement/Placement.h"

#include "app/tools/tools.h"

namespace download {
namespace installer {

std::string Placement::Directory(const std::string& instance_dir, Kind kind) {
    switch (kind) {
        case Kind::Mod: return tools::JoinPath(instance_dir, "mods");
        case Kind::Shaderpack: return tools::JoinPath(instance_dir, "shaderpacks");
        case Kind::Resourcepack: return tools::JoinPath(instance_dir, "resourcepacks");
        case Kind::Datapack: return tools::JoinPath(instance_dir, "datapacks");
        case Kind::World: return tools::JoinPath(instance_dir, "saves");
    }
    return instance_dir;
}

std::string Placement::Resolve(const std::string& instance_dir, Kind kind,
                               const std::string& filename, bool overwrite) {
    const std::string directory = Directory(instance_dir, kind);
    std::string candidate = tools::JoinPath(directory, filename);
    if (overwrite || !tools::PathExists(candidate)) return candidate;

    const size_t dot = filename.find_last_of('.');
    const std::string stem = dot == std::string::npos ? filename : filename.substr(0, dot);
    const std::string extension = dot == std::string::npos ? std::string() : filename.substr(dot);

    for (int index = 1; index < 1000; ++index) {
        candidate = tools::JoinPath(directory, stem + " (" + std::to_string(index) + ")" + extension);
        if (!tools::PathExists(candidate)) return candidate;
    }
    return tools::JoinPath(directory, filename);
}

}  // namespace installer
}  // namespace download
