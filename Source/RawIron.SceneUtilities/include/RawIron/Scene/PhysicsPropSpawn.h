#pragma once

#include "RawIron/Scene/LevelObjectRegistry.h"
#include "RawIron/Scene/PrimitiveTypeCanonical.h"
#include "RawIron/Scene/RuntimeMeshFactory.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Trace/TraceScene.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ri::scene {

/// Authoring payload for a simple physics/visual prop (primitive mesh + optional point light + trace payload).
struct PhysicsPropAuthoring {
    std::optional<std::string> primitiveTypeField{};
    std::optional<std::string> typeAliasField{};
    PrimitiveType primitiveFallback = PrimitiveType::Cube;

    RuntimePrimitiveParams visual{};
    bool attachPointLight = false;
    ri::math::Vec3 lightColor{1.0f, 1.0f, 1.0f};
    float lightIntensity = 2.0f;
    float lightRange = 6.0f;

    bool spawnTraceCollider = true;
    bool traceStructural = true;
    bool traceDynamic = false;
    std::vector<std::string> simulationTags{};
    std::uint32_t simulationFlags = 0U;
};

struct PhysicsPropSpawnResult {
    int visualNode = kInvalidHandle;
    int lightNode = kInvalidHandle;
    std::optional<ri::trace::TraceCollider> traceCollider{};
};

/// Builds scene nodes from authoring; optionally fills a trace collider descriptor (merge into your game's collider vector).
[[nodiscard]] PhysicsPropSpawnResult SpawnPhysicsPropFromAuthoring(Scene& scene,
                                                                   const PhysicsPropAuthoring& authoring,
                                                                   LevelObjectRegistry* registry = nullptr);

} // namespace ri::scene
