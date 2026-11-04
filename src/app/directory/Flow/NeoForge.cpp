#include "app/directory/Flow/NeoForge.h"

#include "app/directory/Flow/Profile.h"
#include "app/directory/Flow/Support.h"

namespace directory {
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
                          download::versionsource::NeoForge::InstallerUrl(
                              context.sources.Origins().NeoForgeRoot(), version),
                          progress, outcome, error)) {
        return false;
    }

    outcome.loader_version = version;
    return true;
}

}  // namespace flow
}  // namespace directory
