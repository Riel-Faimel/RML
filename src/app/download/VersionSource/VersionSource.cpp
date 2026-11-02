#include "app/download/VersionSource/VersionSource.h"

#include "app/tools/tools.h"

namespace download {

VersionSource::VersionSource()
    : sources_(tools::JoinPath(tools::DataRoot(), "sources.json")),
      cache_(tools::JoinPath(tools::DataRoot(), "cache")),
      mojang_(cache_, sources_),
      loader_(cache_, sources_),
      forge_(cache_, sources_),
      neoforge_(cache_, sources_),
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
