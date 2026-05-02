#pragma once

#include "RawIron/Math/Vec3.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace ri::trace {

enum class ImpactDecalKind : std::uint8_t {
    Splatter = 0,
    DragStreak = 1,
};

/// Surface/material hints for pooling shaders or projected decal variants (opaque routing).
enum class DecalSurfaceCategory : std::uint8_t {
    Default = 0,
    Porous,
    Metal,
};

struct ImpactDecalPlacement {
    ImpactDecalKind kind = ImpactDecalKind::Splatter;
    ri::math::Vec3 origin{};
    /// Orthonormal tangent axes on the decal plane; half extents stretch along each axis from `origin`.
    ri::math::Vec3 axisU{};
    ri::math::Vec3 axisV{};
    float halfExtentU = 0.12f;
    float halfExtentV = 0.12f;
};

struct DecalSpawnBudget {
    std::size_t maxDecals = 256U;
    std::size_t activeDecals = 0U;
    /// Returns true if a new decal may be committed; does not increment `activeDecals` (call `Commit` / `Release`).
    [[nodiscard]] bool CanSpawn() const noexcept {
        return activeDecals < maxDecals;
    }
    [[nodiscard]] bool TryCommit() noexcept {
        if (!CanSpawn()) {
            return false;
        }
        ++activeDecals;
        return true;
    }
    void ReleaseOne() noexcept {
        if (activeDecals > 0U) {
            --activeDecals;
        }
    }
};

/// Splatter sits on the impact plane with its plane normal facing `surfaceNormal`.
[[nodiscard]] std::optional<ImpactDecalPlacement> BuildSplatterDecalPlacement(const ri::math::Vec3& hitPosition,
                                                                              const ri::math::Vec3& surfaceNormal,
                                                                              float radius,
                                                                              float surfaceOffset = 0.004f) noexcept;

/// Drag streak aligns its long axis with velocity projected onto the surface; streak half-length scales as
/// `clamp(baseHalfLength * (speed/referenceSpeed), minHalfLength, maxHalfLength)`.
[[nodiscard]] std::optional<ImpactDecalPlacement> BuildDragStreakDecalPlacement(const ri::math::Vec3& hitPosition,
                                                                                const ri::math::Vec3& surfaceNormal,
                                                                                const ri::math::Vec3& velocityWorld,
                                                                                float streakHalfWidth,
                                                                                float baseHalfLength,
                                                                                float referenceSpeed,
                                                                                float minHalfLength,
                                                                                float maxHalfLength,
                                                                                float surfaceOffset = 0.004f) noexcept;

/// Authoring-time decal preset → material/shader routing hints (browser `decalPreset` parity for native UI).
struct DecalAuthoringMaterialHints {
    float alphaTest = 0.12f;
    float metalness = 0.5f;
    float roughness = 0.8f;
    float emissiveIntensity = 0.0f;
    /// Texture tile rate in repeats per meter for the U/V axes; 0 disables auto-scaling so authoring
    /// keeps any explicit `uvScale`. Tooling derives final `uvScale` as `round(scaleAxis * perMeter)`.
    float uvScalePerMeterU = 0.0f;
    float uvScalePerMeterV = 0.0f;
    bool transparent = true;
    bool depthWrite = false;
};

/// Returns hints for known presets (`blood`, `cable`, `hazard`); empty optional if unknown.
[[nodiscard]] std::optional<DecalAuthoringMaterialHints> ResolveAuthoringDecalPreset(std::string_view preset) noexcept;

/// Stable list of preset ids exposed by `ResolveAuthoringDecalPreset` (deterministic order).
[[nodiscard]] std::span<const std::string_view> ListAuthoringDecalPresetIds() noexcept;

} // namespace ri::trace
