#include "app/download/VersionSource/VersionSource.h"

#include "app/tools/tools.h"

namespace download {

VersionSource::VersionSource()
    : cache_(tools::JoinPath(tools::DataRoot(), "cache")),
      mojang_(cache_),
      loader_(cache_),
      modrinth_(cache_),
      curseforge_(cache_) {}

std::vector<ContentSource*> VersionSource::Sources() {
    return {&modrinth_, &curseforge_};
}

ContentSource* VersionSource::Source(const std::string& id) {
    for (auto* source : Sources()) {
        if (source->Id() == id) return source;
    }
    return nullptr;
}

}  // namespace download
