#include "app/download/Flow/Forge.h"

#include "app/download/Flow/Profile.h"
#include "app/download/Flow/Support.h"

namespace download {
namespace flow {

bool Forge::RunImpl(const Context& context, const Request& request, const Progress& progress,
                    Outcome& outcome, std::string& error) {
    std::string version;
    if (!context.sources.Forges().Latest(request.mc_version, version, error)) return false;

    Support::Report(progress, "Forge " + version, 2);
    if (!Profile::Install(context, request, "Forge", version,
                          versionsource::Forge::InstallerUrl(context.sources.Origins().ForgeRoot(),
                                                             version),
                          progress, outcome, error)) {
        return false;
    }

    outcome.loader_version = version;
    return true;
}

}  // namespace flow
}  // namespace download
