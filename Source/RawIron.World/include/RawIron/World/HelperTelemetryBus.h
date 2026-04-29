#pragma once

// Helper Telemetry Bus — umbrella documentation for development-facing helper observability.
// Handbook: Documentation/02 Engine/Helper Telemetry Bus.md
//
// Primary attachment points in-tree:
//   ri::runtime::RuntimeEventBus  (RawIron/Runtime/RuntimeEventBus.h)
//   ri::world::HelperActivityTracker, ri::world::HelperActivityState  (RawIron/World/Instrumentation.h)
//   ri::world::RuntimeHelperMetricsSnapshot, ri::world::BuildRuntimeHelperMetricsSnapshot  (RawIron/World/RuntimeState.h)
//   ri::world::SummarizeHelperActivity  (RawIron/World/HelperActivitySummary.h)
//   ri::world::EntityIoTracker, ri::runtime::entity_io::EmitEntityIo  (Instrumentation.h, EntityIoTelemetry.h)
//   ri::world::SpatialQueryTracker  (RawIron/World/Instrumentation.h)
