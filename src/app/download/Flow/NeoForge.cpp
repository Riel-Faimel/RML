#include "app/download/Flow/NeoForge.h"

#include "app/download/Flow/Profile.h"
#include "app/download/Flow/Support.h"

namespace download {
namespace flow {

bool NeoForge::RunImpl(const Context& context, const Request& request, const Progress& progress,
                       Outcome& outcome, std::string& error) {
    std::string version = request.loader_version;
    if (version.empty() &&
        !context.sources.NeoForges().Latest(request.mc_version, version, error)) {
        return false;
    }

    Support::Report(progress, "NeoForge " + version, 2);
    if (!Profile::Install(context, request, "NeoForge", version,
                          versionsource::NeoForge::InstallerUrl(
                              context.sources.Origins().NeoForgeRoot(), version),
                          progress, outcome, error)) {
        return false;
    }

    outcome.loader_version = version;
    return true;
}

}  // namespace flow
}  // namespace download
