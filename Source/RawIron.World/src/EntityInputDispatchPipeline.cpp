#include "RawIron/World/EntityInputDispatchPipeline.h"

#include "RawIron/Logic/LogicGraph.h"
#include "RawIron/World/RuntimeState.h"

namespace ri::world {
namespace {

void ToLowerInPlace(std::string& value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
}

} // namespace

std::string EntityInputDispatchPipeline::NormalizeKind(const std::string_view kind) {
    std::string out(kind);
    ToLowerInPlace(out);
    return out;
}

void EntityInputDispatchPipeline::Register(std::string actorKind, Handler handler) {
    if (handler == nullptr) {
        return;
    }
    ToLowerInPlace(actorKind);
    entries_.push_back({std::move(actorKind), std::move(handler)});
}

void EntityInputDispatchPipeline::Clear() noexcept {
    entries_.clear();
}

bool EntityInputDispatchPipeline::TryDispatch(RuntimeEnvironmentService& environment,
                                              ri::logic::LogicGraph& graph,
                                              const std::string_view actorKind,
                                              const std::string_view actorId,
                                              const std::string_view inputName,
                                              const ri::logic::LogicContext& context) const {
    if (actorKind.empty() || entries_.empty()) {
        return false;
    }
    const std::string kind = NormalizeKind(actorKind);
    for (const auto& entry : entries_) {
        if (entry.first == kind) {
            if (entry.second(environment, graph, actorId, inputName, context)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace ri::world
