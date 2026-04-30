#pragma once

#include "RawIron/Logic/LogicTypes.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ri::logic {
class LogicGraph;
}

namespace ri::world {

class RuntimeEnvironmentService;

/// Per-entity-type input handlers called before the built-in \ref RuntimeEnvironmentService::ApplyWorldActorLogicInput
/// switch. Register handlers for kinds like `door`, `spawner`, or custom tags from \ref RuntimeEnvironmentService::TagWorldActorKind.
class EntityInputDispatchPipeline {
public:
    using Handler = std::function<bool(RuntimeEnvironmentService& environment,
                                        ri::logic::LogicGraph& graph,
                                        std::string_view actorId,
                                        std::string_view inputName,
                                        const ri::logic::LogicContext& context)>;

    void Register(std::string actorKind, Handler handler);
    void Clear() noexcept;

    /// Returns true if a handler consumed the input (no further dispatch required for that frame).
    [[nodiscard]] bool TryDispatch(RuntimeEnvironmentService& environment,
                                   ri::logic::LogicGraph& graph,
                                   std::string_view actorKind,
                                   std::string_view actorId,
                                   std::string_view inputName,
                                   const ri::logic::LogicContext& context) const;

private:
    static std::string NormalizeKind(std::string_view kind);
    std::vector<std::pair<std::string, Handler>> entries_;
};

} // namespace ri::world
