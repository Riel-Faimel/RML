#include "app/directory/Flow/Dispatcher.h"

#include "app/directory/Flow/Fabric.h"
#include "app/directory/Flow/Forge.h"
#include "app/directory/Flow/NeoForge.h"
#include "app/directory/Flow/Vanilla.h"

namespace directory {
namespace flow {

bool Dispatcher::Run(const Context& context, const Request& request, const Progress& progress,
                     Outcome& outcome, std::string& error) const {
    if (request.loader == "原版") return Vanilla().Run(context, request, progress, outcome, error);
    if (request.loader == "Fabric") return Fabric().Run(context, request, progress, outcome, error);
    if (request.loader == "Forge") return Forge().Run(context, request, progress, outcome, error);
    if (request.loader == "NeoForge") {
        return NeoForge().Run(context, request, progress, outcome, error);
    }
    error = "不认识的加载器: " + request.loader;
    return false;
}

}  // namespace flow
}  // namespace directory
