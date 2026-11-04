#include "app/download/VersionSource/VersionSource.h"

#include "app/directory/Directory.h"
#include "app/tools/tools.h"

namespace download {

VersionSource::VersionSource()
    : sources_(Directory::SourcesFile()),
      cache_(Directory::CacheRoot()),
      mojang_(cache_, sources_),
      loader_(cache_, sources_),
      forge_(cache_, sources_),
      neoforge_(cache_, sources_),
      modrinth_(cache_),
      curseforge_(cache_) {}

std::vector<ContentSource*> VersionSource::Sources() {
    return {&modrinth_, &curseforge_};
}

}  // namespace download
