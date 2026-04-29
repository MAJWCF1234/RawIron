#include "RawIron/Games/RuntimeDiagnosticsStandaloneDraw.h"

#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Spatial/Aabb.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_set>

namespace ri::games {
namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Quat {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float w = 1.f;
};

Quat NormalizeQuat(Quat q) {
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (!std::isfinite(n) || n < 1.0e-8f) {
        return {0.f, 0.f, 0.f, 1.f};
    }
    const float inv = 1.0f / n;
    return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

Quat QuatBetweenUnitYAnd(const ri::math::Vec3& toUnit) {
    constexpr ri::math::Vec3 from{0.f, 1.f, 0.f};
    const float d = std::clamp(ri::math::Dot(from, toUnit), -1.f, 1.f);
    if (d > 0.99999f) {
        return {0.f, 0.f, 0.f, 1.f};
    }
    if (d < -0.99999f) {
        return NormalizeQuat(Quat{1.f, 0.f, 0.f, 0.f});
    }
    const ri::math::Vec3 c = ri::math::Cross(from, toUnit);
    return NormalizeQuat(Quat{c.x, c.y, c.z, 1.f + d});
}

ri::math::Vec3 EulerDegreesFromQuaternion(const Quat& qIn) {
    const Quat q = NormalizeQuat(qIn);
    const float sinr_cosp = 2.f * (q.w * q.x + q.y * q.z);
    const float cosr_cosp = 1.f - 2.f * (q.x * q.x + q.y * q.y);
    const float roll = std::atan2(sinr_cosp, cosr_cosp);

    const float sinp = 2.f * (q.w * q.y - q.z * q.x);
    float pitch = 0.f;
    if (std::fabs(sinp) >= 0.999999f) {
        pitch = std::copysign(kPi * 0.5f, sinp);
    } else {
        pitch = std::asin(sinp);
    }

    const float siny_cosp = 2.f * (q.w * q.z + q.x * q.y);
    const float cosy_cosp = 1.f - 2.f * (q.y * q.y + q.z * q.z);
    const float yaw = std::atan2(siny_cosp, cosy_cosp);

    return ri::math::Vec3{
        ri::math::RadiansToDegrees(roll),
        ri::math::RadiansToDegrees(pitch),
        ri::math::RadiansToDegrees(yaw),
    };
}

std::uint32_t Fnv1aHash(std::string_view text) {
    std::uint32_t hash = 2166136261U;
    for (const unsigned char byte : text) {
        hash ^= static_cast<std::uint32_t>(byte);
        hash *= 16777619U;
    }
    return hash;
}

[[nodiscard]] std::string LowerAscii(std::string_view value) {
    std::string lowered(value);
    for (char& ch : lowered) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lowered;
}

[[nodiscard]] bool ContainsToken(std::string_view value, std::string_view token) {
    return LowerAscii(value).find(std::string(token)) != std::string::npos;
}

ri::math::Vec3 DiagnosticBaseColor(const ri::world::RuntimeDiagnosticsHelper& helper) {
    if (helper.sourceType == "structural_collider") {
        return ri::math::Vec3{0.25f, 0.85f, 1.0f};
    }
    const std::uint32_t h = Fnv1aHash(helper.sourceType.empty() ? std::string_view{"volume"} : std::string_view{helper.sourceType});
    const float r = 0.22f + 0.55f * static_cast<float>((h >> 0U) & 255U) / 255.0f;
    const float g = 0.22f + 0.55f * static_cast<float>((h >> 8U) & 255U) / 255.0f;
    const float b = 0.22f + 0.55f * static_cast<float>((h >> 16U) & 255U) / 255.0f;
    return ri::math::Vec3{r, g, b};
}

void HideNode(ri::scene::Scene& scene, const int nodeHandle) {
    if (nodeHandle == ri::scene::kInvalidHandle) {
        return;
    }
    scene.GetNode(nodeHandle).localTransform.scale = ri::math::Vec3{0.f, 0.f, 0.f};
}

int AddDiagnosticPrimitive(ri::scene::Scene& scene,
                           const int parent,
                           const std::string& nodeName,
                           const ri::scene::PrimitiveType primitive,
                           const ri::math::Vec3& baseColor) {
    ri::scene::PrimitiveNodeOptions helper{};
    helper.nodeName = nodeName;
    helper.parent = parent;
    helper.primitive = primitive;
    helper.materialName = nodeName + "_Material";
    helper.shadingModel = ri::scene::ShadingModel::Unlit;
    helper.baseColor = baseColor;
    helper.emissiveColor = ri::math::Vec3{
        baseColor.x * 0.12f,
        baseColor.y * 0.12f,
        baseColor.z * 0.12f,
    };
    helper.opacity = 0.24f;
    helper.alphaCutoff = 0.f;
    helper.doubleSided = true;
    helper.transparent = true;
    return ri::scene::AddPrimitiveNode(scene, helper);
}

} // namespace

void SeedStandaloneDiagnosticsEnvironmentFromColliders(const std::vector<ri::trace::TraceCollider>& colliders,
                                                       ri::world::RuntimeEnvironmentService& environment) {
    std::vector<ri::world::GenericTriggerVolume> triggers = environment.GetGenericTriggerVolumes();
    std::unordered_set<std::string> existingIds;
    existingIds.reserve(triggers.size() + colliders.size());
    for (const ri::world::GenericTriggerVolume& existing : triggers) {
        existingIds.insert(existing.id);
    }

    ri::math::Vec3 diagMin{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 diagMax{0.0f, 0.0f, 0.0f};
    bool boundsInitialized = false;

    for (std::size_t index = 0; index < colliders.size(); ++index) {
        const ri::trace::TraceCollider& collider = colliders[index];
        const std::string seededId = "__diag_collider_" + (collider.id.empty() ? std::to_string(index) : collider.id);
        if (existingIds.contains(seededId)) {
            continue;
        }

        ri::world::GenericTriggerVolume volume{};
        volume.id = seededId;
        volume.type = "structural_collider";
        volume.debugVisible = true;
        volume.shape = ri::world::VolumeShape::Box;
        const ri::math::Vec3 center = ri::spatial::Center(collider.bounds);
        const ri::math::Vec3 size = collider.bounds.max - collider.bounds.min;
        volume.position = center;
        volume.size = ri::math::Vec3{
            std::max(size.x, 0.01f),
            std::max(size.y, 0.01f),
            std::max(size.z, 0.01f),
        };
        triggers.push_back(std::move(volume));
        existingIds.insert(seededId);

        if (!boundsInitialized) {
            diagMin = collider.bounds.min;
            diagMax = collider.bounds.max;
            boundsInitialized = true;
        } else {
            diagMin.x = std::min(diagMin.x, collider.bounds.min.x);
            diagMin.y = std::min(diagMin.y, collider.bounds.min.y);
            diagMin.z = std::min(diagMin.z, collider.bounds.min.z);
            diagMax.x = std::max(diagMax.x, collider.bounds.max.x);
            diagMax.y = std::max(diagMax.y, collider.bounds.max.y);
            diagMax.z = std::max(diagMax.z, collider.bounds.max.z);
        }
    }
    environment.SetGenericTriggerVolumes(std::move(triggers));

    std::vector<ri::world::VisibilityPrimitive> visibility = environment.GetVisibilityPrimitives();
    std::unordered_set<std::string> visibilityIds{};
    visibilityIds.reserve(visibility.size() + colliders.size());
    for (const ri::world::VisibilityPrimitive& existing : visibility) {
        visibilityIds.insert(existing.id);
    }

    std::vector<ri::world::DamageTriggerVolume> damageVolumes = environment.GetDamageVolumes();
    std::unordered_set<std::string> damageIds{};
    damageIds.reserve(damageVolumes.size() + colliders.size());
    for (const ri::world::DamageTriggerVolume& existing : damageVolumes) {
        damageIds.insert(existing.id);
    }

    if (!boundsInitialized) {
        environment.SetVisibilityPrimitives(std::move(visibility));
        environment.SetDamageVolumes(std::move(damageVolumes));
        return;
    }

    for (std::size_t index = 0; index < colliders.size(); ++index) {
        const ri::trace::TraceCollider& collider = colliders[index];
        const std::string loweredId = LowerAscii(collider.id);
        const ri::math::Vec3 center = ri::spatial::Center(collider.bounds);
        const ri::math::Vec3 size = collider.bounds.max - collider.bounds.min;

        if (ContainsToken(loweredId, "portal") || ContainsToken(loweredId, "anti_portal")
            || ContainsToken(loweredId, "antiportal")) {
            ri::world::VisibilityPrimitive primitive{};
            primitive.id = "__diag_visibility_" + (collider.id.empty() ? std::to_string(index) : collider.id);
            if (!visibilityIds.contains(primitive.id)) {
                const bool antiPortal = ContainsToken(loweredId, "anti_portal") || ContainsToken(loweredId, "antiportal");
                primitive.type = antiPortal ? "anti_portal" : "portal";
                primitive.kind = antiPortal
                    ? ri::world::VisibilityPrimitiveKind::AntiPortal
                    : ri::world::VisibilityPrimitiveKind::Portal;
                primitive.position = center;
                primitive.size = ri::math::Vec3{
                    std::max(size.x, 0.01f),
                    std::max(size.y, 0.01f),
                    std::max(size.z, 0.01f),
                };
                visibility.push_back(std::move(primitive));
                visibilityIds.insert(visibility.back().id);
            }
        }

        if (ContainsToken(loweredId, "damage") || ContainsToken(loweredId, "kill")
            || ContainsToken(loweredId, "death") || ContainsToken(loweredId, "acid")) {
            ri::world::DamageTriggerVolume volume{};
            volume.id = "__diag_damage_" + (collider.id.empty() ? std::to_string(index) : collider.id);
            if (!damageIds.contains(volume.id)) {
                volume.type = ContainsToken(loweredId, "kill") || ContainsToken(loweredId, "death")
                    ? "kill_volume"
                    : "damage_volume";
                volume.shape = ri::world::VolumeShape::Box;
                volume.position = center;
                volume.size = ri::math::Vec3{
                    std::max(size.x, 0.01f),
                    std::max(size.y, 0.01f),
                    std::max(size.z, 0.01f),
                };
                volume.killInstant = ContainsToken(loweredId, "kill") || ContainsToken(loweredId, "death");
                volume.damagePerSecond = volume.killInstant ? 9999.0f : 20.0f;
                volume.label = collider.id;
                damageVolumes.push_back(std::move(volume));
                damageIds.insert(damageVolumes.back().id);
            }
        }
    }
    environment.SetVisibilityPrimitives(std::move(visibility));
    environment.SetDamageVolumes(std::move(damageVolumes));

    auto upsertLightImportance = [&diagMin, &diagMax](std::vector<ri::world::LightImportanceVolume>& volumes,
                                                      const std::string& id,
                                                      const bool probeGridBounds) {
        ri::world::LightImportanceVolume seeded{};
        seeded.id = id;
        seeded.type = probeGridBounds ? "probe_grid_bounds" : "light_importance_volume";
        seeded.debugVisible = true;
        seeded.shape = ri::world::VolumeShape::Box;
        seeded.position = (diagMin + diagMax) * 0.5f;
        const ri::math::Vec3 size = diagMax - diagMin;
        seeded.size = ri::math::Vec3{
            std::max(size.x, 0.01f),
            std::max(size.y, 0.01f),
            std::max(size.z, 0.01f),
        };
        seeded.probeGridBounds = probeGridBounds;

        const auto it = std::find_if(volumes.begin(), volumes.end(), [&id](const ri::world::LightImportanceVolume& v) {
            return v.id == id;
        });
        if (it == volumes.end()) {
            volumes.push_back(std::move(seeded));
        } else {
            *it = std::move(seeded);
        }
    };

    std::vector<ri::world::LightImportanceVolume> lightImportance = environment.GetLightImportanceVolumes();
    upsertLightImportance(lightImportance, "__diag_light_importance_volume", false);
    upsertLightImportance(lightImportance, "__diag_probe_grid_bounds", true);
    environment.SetLightImportanceVolumes(std::move(lightImportance));
}

void SyncStandaloneRuntimeDiagnosticsScene(ri::scene::Scene& scene,
                                          const int parentSceneRoot,
                                          int& diagnosticsRoot,
                                          std::vector<int>& volumeNodes,
                                          std::vector<int>& gizmoNodes,
                                          const ri::world::RuntimeDiagnosticsSnapshot& snapshot) {
    if (diagnosticsRoot == ri::scene::kInvalidHandle) {
        diagnosticsRoot = scene.CreateNode("RuntimeDiagnosticsRoot", parentSceneRoot);
    }

    const std::size_t count = snapshot.helpers.size();
    while (volumeNodes.size() < count) {
        const std::size_t index = volumeNodes.size();
        const ri::world::RuntimeDiagnosticsHelper& helper = snapshot.helpers[index];
        const ri::math::Vec3 color = DiagnosticBaseColor(helper);
        const ri::scene::PrimitiveType primitive =
            helper.volume.shape == ri::world::VolumeShape::Sphere ? ri::scene::PrimitiveType::Sphere : ri::scene::PrimitiveType::Cube;
        volumeNodes.push_back(AddDiagnosticPrimitive(
            scene,
            diagnosticsRoot,
            "RuntimeDiagVol_" + std::to_string(index),
            primitive,
            color));
        ri::scene::PrimitiveNodeOptions gizmoOpts{};
        gizmoOpts.nodeName = "RuntimeDiagGizmo_" + std::to_string(index);
        gizmoOpts.parent = diagnosticsRoot;
        gizmoOpts.primitive = ri::scene::PrimitiveType::Cube;
        gizmoOpts.materialName = gizmoOpts.nodeName + "_Material";
        gizmoOpts.shadingModel = ri::scene::ShadingModel::Unlit;
        gizmoOpts.baseColor = ri::math::Vec3{1.0f, 0.55f, 0.12f};
        gizmoOpts.emissiveColor = ri::math::Vec3{0.35f, 0.18f, 0.04f};
        gizmoOpts.opacity = 0.55f;
        gizmoOpts.alphaCutoff = 0.f;
        gizmoOpts.doubleSided = true;
        gizmoOpts.transparent = true;
        gizmoNodes.push_back(ri::scene::AddPrimitiveNode(scene, gizmoOpts));
    }

    for (std::size_t index = 0; index < volumeNodes.size(); ++index) {
        if (index >= count) {
            HideNode(scene, volumeNodes[index]);
            HideNode(scene, gizmoNodes[index]);
            continue;
        }

        const ri::world::RuntimeDiagnosticsHelper& helper = snapshot.helpers[index];
        ri::scene::Node& volumeNode = scene.GetNode(volumeNodes[index]);
        const ri::world::RuntimeVolume& vol = helper.volume;
        volumeNode.localTransform.position = vol.position;
        volumeNode.localTransform.rotationDegrees = ri::math::Vec3{0.f, 0.f, 0.f};

        switch (vol.shape) {
        case ri::world::VolumeShape::Sphere: {
            const float diameter = std::max(vol.radius, 0.02f) * 2.0f;
            volumeNode.localTransform.scale = ri::math::Vec3{diameter, diameter, diameter};
            break;
        }
        case ri::world::VolumeShape::Cylinder: {
            const float diameter = std::max(vol.radius, 0.02f) * 2.0f;
            const float height = std::max(vol.height, 0.02f);
            volumeNode.localTransform.scale = ri::math::Vec3{diameter, height, diameter};
            break;
        }
        case ri::world::VolumeShape::Box:
        default: {
            volumeNode.localTransform.scale = ri::math::Vec3{
                std::max(vol.size.x, 0.01f),
                std::max(vol.size.y, 0.01f),
                std::max(vol.size.z, 0.01f),
            };
            break;
        }
        }

        ri::scene::Node& gizmoNode = scene.GetNode(gizmoNodes[index]);
        if (helper.gizmoLineStart.has_value() && helper.gizmoLineEnd.has_value()) {
            const ri::math::Vec3 a = *helper.gizmoLineStart;
            const ri::math::Vec3 b = *helper.gizmoLineEnd;
            const float len = ri::math::Distance(a, b);
            if (len < 1.0e-4f || !std::isfinite(len)) {
                HideNode(scene, gizmoNodes[index]);
            } else {
                const ri::math::Vec3 dir = ri::math::Normalize(b - a);
                const Quat q = QuatBetweenUnitYAnd(dir);
                gizmoNode.localTransform.position = ri::math::Lerp(a, b, 0.5f);
                gizmoNode.localTransform.rotationDegrees = EulerDegreesFromQuaternion(q);
                const float thickness = 0.045f;
                gizmoNode.localTransform.scale = ri::math::Vec3{thickness, std::max(len, 0.04f), thickness};
            }
        } else {
            HideNode(scene, gizmoNodes[index]);
        }
    }
}

void HideStandaloneRuntimeDiagnosticsScene(ri::scene::Scene& scene,
                                         const std::vector<int>& volumeNodes,
                                         const std::vector<int>& gizmoNodes) {
    for (const int handle : volumeNodes) {
        HideNode(scene, handle);
    }
    for (const int handle : gizmoNodes) {
        HideNode(scene, handle);
    }
}

} // namespace ri::games
