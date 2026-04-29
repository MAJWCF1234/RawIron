#pragma once

#include "RawIron/Trace/TraceScene.h"
#include "RawIron/World/RuntimeState.h"

#include <vector>

namespace ri::scene {
class Scene;
}

namespace ri::games {

/// Seeds debug helper volumes from structural trace colliders so `RuntimeDiagnosticsLayer` has
/// something to visualize before authored environment volumes are loaded.
/// Preserves authored environment entries and only appends diagnostic-only records.
void SeedStandaloneDiagnosticsEnvironmentFromColliders(const std::vector<ri::trace::TraceCollider>& colliders,
                                                       ri::world::RuntimeEnvironmentService& environment);

/// Creates/updates translucent primitive helpers under `diagnosticsRoot` from a diagnostics snapshot.
void SyncStandaloneRuntimeDiagnosticsScene(ri::scene::Scene& scene,
                                            int parentSceneRoot,
                                            int& diagnosticsRoot,
                                            std::vector<int>& volumeNodes,
                                            std::vector<int>& gizmoNodes,
                                            const ri::world::RuntimeDiagnosticsSnapshot& snapshot);

void HideStandaloneRuntimeDiagnosticsScene(ri::scene::Scene& scene,
                                           const std::vector<int>& volumeNodes,
                                           const std::vector<int>& gizmoNodes);

} // namespace ri::games
