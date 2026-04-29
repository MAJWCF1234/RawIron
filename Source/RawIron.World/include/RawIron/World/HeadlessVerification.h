#pragma once

#include <cstdint>

namespace ri::world::headless {

// Headless/state-level verification: deterministic assertions on structured outputs without
// rendering or live I/O. Subsystems keep rich typed bundles first; optional numeric codes second.
//
// Numeric bands are stable serial ranges for tests, telemetry, and snapshot diffs. Do not
// renumber released values; add new codes within the band or open a new band.
//
// Layout:
//   0x01xx — NpcAgentState interaction outcomes (NpcInteractionOutcomeCode)
//   0x02xx — NpcAgentState patrol phases (NpcPatrolPhaseCode)
//   0x03xx — (reserved) next subsystem
//
inline constexpr std::uint16_t kNpcInteractionOutcomeBand = 0x0100;
inline constexpr std::uint16_t kNpcPatrolPhaseBand = 0x0200;

// Scenario runner (apply + read + match rows): HeadlessModuleVerifier.h → RunHeadlessSuccessChart.

} // namespace ri::world::headless
