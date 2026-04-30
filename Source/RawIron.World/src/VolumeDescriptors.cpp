#include "RawIron/World/VolumeDescriptors.h"

#include "RawIron/Runtime/RuntimeId.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <unordered_set>

namespace ri::world {
namespace {

std::string NormalizeToken(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (char character : value) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return normalized;
}

template <typename T>
bool Contains(const std::vector<T>& values, const T& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

float ClampFiniteFloat(float value, float fallback, float minValue, float maxValue) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, minValue, maxValue);
}

ri::math::Vec3 SanitizeFlow(const ri::math::Vec3& flow) {
    return ri::math::Vec3{
        std::isfinite(flow.x) ? flow.x : 0.0f,
        std::isfinite(flow.y) ? flow.y : 0.0f,
        std::isfinite(flow.z) ? flow.z : 0.0f,
    };
}

ri::math::Vec3 SanitizeVector(const ri::math::Vec3& value, const ri::math::Vec3& fallback = {}) {
    return ri::math::Vec3{
        std::isfinite(value.x) ? value.x : fallback.x,
        std::isfinite(value.y) ? value.y : fallback.y,
        std::isfinite(value.z) ? value.z : fallback.z,
    };
}

float PositiveExtent(float value) {
    return std::fabs(std::isfinite(value) ? value : 0.0f);
}

std::optional<ClipVolumeMode> ParseClipVolumeModeToken(std::string_view token) {
    const std::string normalized = NormalizeToken(token);
    if (normalized == "physics" || normalized == "collision") {
        return ClipVolumeMode::Physics;
    }
    if (normalized == "ai") {
        return ClipVolumeMode::AI;
    }
    if (normalized == "visibility") {
        return ClipVolumeMode::Visibility;
    }
    return std::nullopt;
}

std::optional<CollisionChannel> ParseCollisionChannelToken(std::string_view token) {
    const std::string normalized = NormalizeToken(token);
    if (normalized == "player" || normalized == "pawn" || normalized == "character") {
        return CollisionChannel::Player;
    }
    if (normalized == "physics" || normalized == "world" || normalized == "solid") {
        return CollisionChannel::Physics;
    }
    if (normalized == "camera" || normalized == "view") {
        return CollisionChannel::Camera;
    }
    if (normalized == "bullet" || normalized == "projectile" || normalized == "weapon") {
        return CollisionChannel::Bullet;
    }
    if (normalized == "ai" || normalized == "npc" || normalized == "agent") {
        return CollisionChannel::AI;
    }
    if (normalized == "vehicle" || normalized == "car") {
        return CollisionChannel::Vehicle;
    }
    return std::nullopt;
}

std::uint32_t CollisionChannelBit(const CollisionChannel channel) {
    return 1U << static_cast<std::uint32_t>(channel);
}

std::optional<ConstraintAxis> ParseConstraintAxisToken(std::string_view token) {
    const std::string normalized = NormalizeToken(token);
    if (normalized == "x") {
        return ConstraintAxis::X;
    }
    if (normalized == "y") {
        return ConstraintAxis::Y;
    }
    if (normalized == "z") {
        return ConstraintAxis::Z;
    }
    return std::nullopt;
}

} // namespace

std::string ToString(ClipVolumeMode mode) {
    switch (mode) {
    case ClipVolumeMode::Physics:
        return "physics";
    case ClipVolumeMode::AI:
        return "ai";
    case ClipVolumeMode::Visibility:
        return "visibility";
    }
    return "physics";
}

std::string ToString(CollisionChannel channel) {
    switch (channel) {
    case CollisionChannel::Player:
        return "player";
    case CollisionChannel::Physics:
        return "physics";
    case CollisionChannel::Camera:
        return "camera";
    case CollisionChannel::Bullet:
        return "bullet";
    case CollisionChannel::AI:
        return "ai";
    case CollisionChannel::Vehicle:
        return "vehicle";
    }
    return "player";
}

std::string ToString(ConstraintAxis axis) {
    switch (axis) {
    case ConstraintAxis::X:
        return "x";
    case ConstraintAxis::Y:
        return "y";
    case ConstraintAxis::Z:
        return "z";
    }
    return "x";
}

std::string ToString(TraversalLinkKind kind) {
    switch (kind) {
    case TraversalLinkKind::General:
        return "traversal_link_volume";
    case TraversalLinkKind::Ladder:
        return "ladder_volume";
    case TraversalLinkKind::Climb:
        return "climb_volume";
    }
    return "traversal_link_volume";
}

std::string ToString(HintPartitionMode mode) {
    switch (mode) {
    case HintPartitionMode::Hint:
        return "hint";
    case HintPartitionMode::Skip:
        return "skip";
    }
    return "hint";
}

std::string ToString(ForcedLod forcedLod) {
    switch (forcedLod) {
    case ForcedLod::Near:
        return "near";
    case ForcedLod::Far:
        return "far";
    }
    return "near";
}

std::string ToString(VisibilityPrimitiveKind kind) {
    switch (kind) {
    case VisibilityPrimitiveKind::Portal:
        return "portal";
    case VisibilityPrimitiveKind::AntiPortal:
        return "anti_portal";
    case VisibilityPrimitiveKind::OcclusionPortal:
        return "occlusion_portal";
    }
    return "portal";
}

std::string ToString(TriggerTransitionKind kind) {
    switch (kind) {
    case TriggerTransitionKind::Enter:
        return "enter";
    case TriggerTransitionKind::Stay:
        return "stay";
    case TriggerTransitionKind::Exit:
        return "exit";
    }
    return "enter";
}

std::vector<ClipVolumeMode> ParseClipVolumeModes(const std::vector<std::string>& rawModes) {
    std::vector<ClipVolumeMode> parsed;
    parsed.reserve(rawModes.size());

    for (const std::string& rawMode : rawModes) {
        const std::optional<ClipVolumeMode> mode = ParseClipVolumeModeToken(rawMode);
        if (!mode.has_value() || Contains(parsed, *mode)) {
            continue;
        }
        parsed.push_back(*mode);
    }

    if (parsed.empty()) {
        parsed.push_back(ClipVolumeMode::Physics);
    }
    return parsed;
}

ClipInteractionModeResolveResult ResolveClipInteractionModes(const std::vector<std::string>& rawModes) {
    ClipInteractionModeResolveResult out;
    for (const std::string& rawMode : rawModes) {
        const std::optional<ClipVolumeMode> mode = ParseClipVolumeModeToken(rawMode);
        if (mode.has_value()) {
            if (!Contains(out.modes, *mode)) {
                out.modes.push_back(*mode);
            }
        } else if (!rawMode.empty()) {
            out.unknownTokens.push_back(rawMode);
        }
    }
    if (out.modes.empty()) {
        out.modes.push_back(ClipVolumeMode::Physics);
        out.usedDefault = true;
    }
    return out;
}

std::vector<CollisionChannel> ParseCollisionChannels(const std::vector<std::string>& rawChannels) {
    return ResolveCollisionChannelAuthoring(rawChannels).channels;
}

std::vector<ConstraintAxis> ParseConstraintAxes(const std::vector<std::string>& rawAxes) {
    std::vector<ConstraintAxis> parsed;
    parsed.reserve(rawAxes.size());

    for (const std::string& rawAxis : rawAxes) {
        const std::string normalized = NormalizeToken(rawAxis);
        if (normalized == "xy" || normalized == "yx") {
            if (!Contains(parsed, ConstraintAxis::X)) {
                parsed.push_back(ConstraintAxis::X);
            }
            if (!Contains(parsed, ConstraintAxis::Y)) {
                parsed.push_back(ConstraintAxis::Y);
            }
            continue;
        }
        if (normalized == "xz" || normalized == "zx") {
            if (!Contains(parsed, ConstraintAxis::X)) {
                parsed.push_back(ConstraintAxis::X);
            }
            if (!Contains(parsed, ConstraintAxis::Z)) {
                parsed.push_back(ConstraintAxis::Z);
            }
            continue;
        }
        if (normalized == "yz" || normalized == "zy") {
            if (!Contains(parsed, ConstraintAxis::Y)) {
                parsed.push_back(ConstraintAxis::Y);
            }
            if (!Contains(parsed, ConstraintAxis::Z)) {
                parsed.push_back(ConstraintAxis::Z);
            }
            continue;
        }

        const std::optional<ConstraintAxis> axis = ParseConstraintAxisToken(rawAxis);
        if (axis.has_value() && !Contains(parsed, *axis)) {
            parsed.push_back(*axis);
        }
    }

    return parsed;
}

std::uint32_t BuildCollisionChannelMask(const std::vector<CollisionChannel>& channels) {
    std::uint32_t mask = 0U;
    for (const CollisionChannel channel : channels) {
        mask |= CollisionChannelBit(channel);
    }
    return mask;
}

CollisionChannelResolveResult ResolveCollisionChannelAuthoring(const std::vector<std::string>& rawChannels,
                                                               const CollisionChannel defaultChannel) {
    CollisionChannelResolveResult result{};
    result.channels.reserve(rawChannels.size());
    for (const std::string& rawChannel : rawChannels) {
        const std::optional<CollisionChannel> channel = ParseCollisionChannelToken(rawChannel);
        if (!channel.has_value()) {
            if (!rawChannel.empty()) {
                result.unknownTokens.push_back(rawChannel);
            }
            continue;
        }
        if (!Contains(result.channels, *channel)) {
            result.channels.push_back(*channel);
        }
    }
    if (result.channels.empty()) {
        result.channels.push_back(defaultChannel);
        result.usedDefault = true;
    }
    result.mask = BuildCollisionChannelMask(result.channels);
    return result;
}

std::vector<std::string> NormalizeTraceTags(const std::vector<std::string>& rawTags) {
    std::vector<std::string> normalized;
    normalized.reserve(rawTags.size());
    std::unordered_set<std::string> seen;
    for (const std::string& rawTag : rawTags) {
        const std::string token = NormalizeToken(rawTag);
        if (token.empty() || seen.contains(token)) {
            continue;
        }
        seen.insert(token);
        normalized.push_back(token);
    }
    std::sort(normalized.begin(), normalized.end());
    return normalized;
}

bool TraceTagMatchesVolume(std::string_view traceTag, const FilteredCollisionVolume& volume) {
    return TraceAndVolumeTagsMatch(traceTag, {}, volume);
}

bool TraceAndVolumeTagsMatch(std::string_view traceTag,
                             const std::vector<std::string>& traceTags,
                             const FilteredCollisionVolume& volume) {
    const CollisionChannel channel = ParseCollisionChannelToken(traceTag).value_or(CollisionChannel::Player);
    const std::uint32_t channelMask = volume.channelMask != 0U
        ? volume.channelMask
        : BuildCollisionChannelMask(volume.channels);
    if ((channelMask & CollisionChannelBit(channel)) == 0U) {
        return false;
    }

    const std::vector<std::string> normalizedTraceTags = NormalizeTraceTags(traceTags);
    if (normalizedTraceTags.empty() && !volume.allowUntaggedTrace) {
        return false;
    }
    for (const std::string& blocked : NormalizeTraceTags(volume.excludeTags)) {
        if (std::find(normalizedTraceTags.begin(), normalizedTraceTags.end(), blocked) != normalizedTraceTags.end()) {
            return false;
        }
    }
    const std::vector<std::string> required = NormalizeTraceTags(volume.includeTags);
    if (required.empty()) {
        return true;
    }
    if (volume.requireAllIncludeTags) {
        for (const std::string& need : required) {
            if (std::find(normalizedTraceTags.begin(), normalizedTraceTags.end(), need) == normalizedTraceTags.end()) {
                return false;
            }
        }
        return true;
    }
    for (const std::string& need : required) {
        if (std::find(normalizedTraceTags.begin(), normalizedTraceTags.end(), need) != normalizedTraceTags.end()) {
            return true;
        }
    }
    return false;
}

RuntimeVolume CreateRuntimeVolume(const RuntimeVolumeSeed& data, const VolumeDefaults& defaults) {
    RuntimeVolume volume{};
    volume.id = data.id.empty() ? ri::runtime::CreateRuntimeId(defaults.runtimeId) : data.id;
    volume.type = data.type.empty() ? defaults.type : data.type;
    volume.debugVisible = data.debugVisible.value_or(true);
    volume.position = data.position.value_or(ri::math::Vec3{0.0f, 0.0f, 0.0f});
    volume.shape = data.shape.value_or(defaults.shape);

    const ri::math::Vec3 rawSize = data.size.value_or(defaults.size);
    volume.size = ri::math::Vec3{
        PositiveExtent(rawSize.x),
        PositiveExtent(rawSize.y),
        PositiveExtent(rawSize.z),
    };

    const float defaultRadius = std::max(volume.size.x, volume.size.z) * 0.5f;
    const float rawHeight = volume.size.y > 0.0f ? volume.size.y : 2.0f;
    volume.radius = std::max(0.25f, data.radius.value_or(defaultRadius));
    volume.height = std::max(0.25f, data.height.value_or(rawHeight));
    return volume;
}

AuthoringRuntimeVolumeRecord BuildAuthoringRuntimeVolumeRecord(const RuntimeVolumeSeed& data,
                                                              const VolumeDefaults& defaults) {
    const RuntimeVolume base = CreateRuntimeVolume(data, defaults);
    AuthoringRuntimeVolumeRecord record{};
    record.id = base.id;
    record.type = base.type;
    record.shape = base.shape;
    record.position = base.position;
    record.rotationRadians = data.rotationRadians;
    record.size = base.size;
    record.radius = base.radius;
    record.height = base.height;
    record.debugVisible = base.debugVisible;
    return record;
}

FilteredCollisionVolume CreateFilteredCollisionVolume(const RuntimeVolumeSeed& data,
                                                      const std::vector<std::string>& rawChannels,
                                                      const VolumeDefaults& defaults) {
    FilteredCollisionVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    const CollisionChannelResolveResult resolved = ResolveCollisionChannelAuthoring(rawChannels);
    volume.channels = resolved.channels;
    volume.channelMask = resolved.mask;
    return volume;
}

InvisibleStructuralProxyVolume CreateInvisibleStructuralProxyVolume(const RuntimeVolumeSeed& data,
                                                                    std::string_view sourceId,
                                                                    float inflation,
                                                                    bool collisionEnabled,
                                                                    bool queryEnabled,
                                                                    bool logicEnabled,
                                                                    const VolumeDefaults& defaults) {
    InvisibleStructuralProxyVolume proxy{};
    static_cast<RuntimeVolume&>(proxy) = CreateRuntimeVolume(data, defaults);
    proxy.type = "invisible_structural_proxy_volume";
    proxy.debugVisible = false;
    proxy.sourceId = sourceId.empty() ? proxy.id : std::string(sourceId);
    proxy.inflation = ClampFiniteFloat(inflation, 0.0f, 0.0f, 1000.0f);
    proxy.collisionEnabled = collisionEnabled;
    proxy.queryEnabled = queryEnabled;
    proxy.logicEnabled = logicEnabled;
    if (proxy.inflation > 0.0f) {
        proxy.size = proxy.size + ri::math::Vec3{proxy.inflation * 2.0f, proxy.inflation * 2.0f, proxy.inflation * 2.0f};
        proxy.radius = std::max(0.001f, proxy.radius + proxy.inflation);
        proxy.height = std::max(0.001f, proxy.height + proxy.inflation * 2.0f);
    }
    return proxy;
}

std::vector<ri::spatial::SpatialEntry> BuildInvisibleStructuralProxySpatialEntries(
    const std::vector<InvisibleStructuralProxyVolume>& proxies) {
    std::vector<ri::spatial::SpatialEntry> entries;
    entries.reserve(proxies.size());
    for (const InvisibleStructuralProxyVolume& proxy : proxies) {
        if (!proxy.queryEnabled && !proxy.collisionEnabled) {
            continue;
        }
        const RuntimeVolume& base = static_cast<const RuntimeVolume&>(proxy);
        ri::spatial::Aabb bounds = ri::spatial::MakeEmptyAabb();
        switch (base.shape) {
        case VolumeShape::Box: {
            const ri::math::Vec3 halfExtents{
                std::max(0.001f, std::fabs(base.size.x) * 0.5f),
                std::max(0.001f, std::fabs(base.size.y) * 0.5f),
                std::max(0.001f, std::fabs(base.size.z) * 0.5f),
            };
            bounds = {.min = base.position - halfExtents, .max = base.position + halfExtents};
            break;
        }
        case VolumeShape::Cylinder: {
            const float radius = std::max(0.001f, std::fabs(std::isfinite(base.radius) ? base.radius : 0.5f));
            const float halfHeight = std::max(
                0.001f,
                std::fabs(std::isfinite(base.height) ? base.height : base.size.y) * 0.5f);
            const ri::math::Vec3 extents{radius, halfHeight, radius};
            bounds = {.min = base.position - extents, .max = base.position + extents};
            break;
        }
        case VolumeShape::Sphere:
        default: {
            const float radius = std::max(0.001f, std::fabs(std::isfinite(base.radius) ? base.radius : 0.5f));
            const ri::math::Vec3 extents{radius, radius, radius};
            bounds = {.min = base.position - extents, .max = base.position + extents};
            break;
        }
        }
        entries.push_back(ri::spatial::SpatialEntry{
            .id = proxy.id,
            .bounds = bounds,
        });
    }
    std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) { return lhs.id < rhs.id; });
    return entries;
}

ClipRuntimeVolume CreateClipRuntimeVolume(const RuntimeVolumeSeed& data,
                                          const std::vector<std::string>& rawModes,
                                          bool enabled,
                                          const VolumeDefaults& defaults) {
    ClipRuntimeVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.modes = ResolveClipInteractionModes(rawModes).modes;
    volume.enabled = enabled;
    return volume;
}

DamageVolume CreateDamageVolume(const RuntimeVolumeSeed& data,
                                float damagePerSecond,
                                bool killInstant,
                                std::string_view label,
                                const VolumeDefaults& defaults) {
    DamageVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.killInstant = killInstant;
    volume.damagePerSecond = killInstant
        ? 9999.0f
        : ClampFiniteFloat(damagePerSecond, 18.0f, 0.1f, 5000.0f);
    volume.label = label.empty() ? volume.id : std::string(label);
    return volume;
}

CameraModifierVolume CreateCameraModifierVolume(const RuntimeVolumeSeed& data,
                                                float fov,
                                                float priority,
                                                float blendDistance,
                                                float shakeAmplitude,
                                                float shakeFrequency,
                                                const ri::math::Vec3& cameraOffset,
                                                const VolumeDefaults& defaults) {
    CameraModifierVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.fov = ClampFiniteFloat(fov, 58.0f, 20.0f, 140.0f);
    volume.priority = ClampFiniteFloat(priority, 0.0f, -100.0f, 100.0f);
    volume.blendDistance = ClampFiniteFloat(blendDistance, 2.0f, 0.0f, 64.0f);
    volume.shakeAmplitude = ClampFiniteFloat(shakeAmplitude, 0.0f, 0.0f, 4.0f);
    volume.shakeFrequency = ClampFiniteFloat(shakeFrequency, 0.0f, 0.0f, 64.0f);
    volume.cameraOffset = SanitizeVector(cameraOffset, {});
    return volume;
}

SafeZoneVolume CreateSafeZoneVolume(const RuntimeVolumeSeed& data,
                                    bool dropAggro,
                                    const VolumeDefaults& defaults) {
    SafeZoneVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.dropAggro = dropAggro;
    return volume;
}

PhysicsModifierVolume CreateCustomGravityVolume(const RuntimeVolumeSeed& data,
                                                float gravityScale,
                                                float jumpScale,
                                                float drag,
                                                float buoyancy,
                                                const ri::math::Vec3& flow,
                                                const VolumeDefaults& defaults) {
    PhysicsModifierVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.gravityScale = ClampFiniteFloat(gravityScale, 0.4f, -2.0f, 4.0f);
    volume.jumpScale = ClampFiniteFloat(jumpScale, 1.0f, 0.0f, 4.0f);
    volume.drag = ClampFiniteFloat(drag, 0.0f, 0.0f, 8.0f);
    volume.buoyancy = ClampFiniteFloat(buoyancy, 0.0f, 0.0f, 3.0f);
    volume.flow = SanitizeFlow(flow);
    return volume;
}

PhysicsModifierVolume CreateDirectionalWindVolume(const RuntimeVolumeSeed& data,
                                                  float drag,
                                                  const ri::math::Vec3& flow,
                                                  const VolumeDefaults& defaults) {
    PhysicsModifierVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.gravityScale = 1.0f;
    volume.jumpScale = 1.0f;
    volume.drag = ClampFiniteFloat(drag, 0.4f, 0.0f, 8.0f);
    volume.buoyancy = 0.0f;
    volume.flow = SanitizeFlow(flow);
    return volume;
}

PhysicsModifierVolume CreateBuoyancyVolume(const RuntimeVolumeSeed& data,
                                           float gravityScale,
                                           float jumpScale,
                                           float drag,
                                           float buoyancy,
                                           const ri::math::Vec3& flow,
                                           const VolumeDefaults& defaults) {
    PhysicsModifierVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.gravityScale = ClampFiniteFloat(gravityScale, 0.8f, -2.0f, 4.0f);
    volume.jumpScale = ClampFiniteFloat(jumpScale, 0.9f, 0.0f, 4.0f);
    volume.drag = ClampFiniteFloat(drag, 1.2f, 0.0f, 8.0f);
    volume.buoyancy = ClampFiniteFloat(buoyancy, 0.85f, 0.0f, 3.0f);
    volume.flow = SanitizeFlow(flow);
    return volume;
}

SurfaceVelocityPrimitive CreateSurfaceVelocityPrimitive(const RuntimeVolumeSeed& data,
                                                        const ri::math::Vec3& flow,
                                                        const VolumeDefaults& defaults) {
    SurfaceVelocityPrimitive volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.flow = SanitizeFlow(flow);
    return volume;
}

WaterSurfacePrimitive CreateWaterSurfacePrimitive(const RuntimeVolumeSeed& data,
                                                  float waveAmplitude,
                                                  float waveFrequency,
                                                  float flowSpeed,
                                                  bool blocksUnderwaterFog,
                                                  const VolumeDefaults& defaults) {
    WaterSurfacePrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.waveAmplitude = ClampFiniteFloat(waveAmplitude, 0.08f, 0.0f, 4.0f);
    primitive.waveFrequency = ClampFiniteFloat(waveFrequency, 0.6f, 0.0f, 12.0f);
    primitive.flowSpeed = ClampFiniteFloat(flowSpeed, 0.0f, -30.0f, 30.0f);
    primitive.blocksUnderwaterFog = blocksUnderwaterFog;
    return primitive;
}

RadialForceVolume CreateRadialForceVolume(const RuntimeVolumeSeed& data,
                                          float strength,
                                          float falloff,
                                          float innerRadius,
                                          const VolumeDefaults& defaults) {
    RadialForceVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.strength = ClampFiniteFloat(strength, 4.2f, -40.0f, 40.0f);
    volume.falloff = ClampFiniteFloat(falloff, 1.0f, 0.0f, 4.0f);
    volume.innerRadius = ClampFiniteFloat(innerRadius, 0.0f, 0.0f, std::max(0.0f, volume.radius - 0.01f));
    return volume;
}

PhysicsConstraintVolume CreatePhysicsConstraintVolume(const RuntimeVolumeSeed& data,
                                                      const std::vector<std::string>& rawLockAxes,
                                                      const VolumeDefaults& defaults) {
    PhysicsConstraintVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.lockAxes = ParseConstraintAxes(rawLockAxes);
    return volume;
}

SplinePathFollowerPrimitive CreateSplinePathFollowerPrimitive(const RuntimeVolumeSeed& data,
                                                              std::vector<ri::math::Vec3> splinePoints,
                                                              float speedUnitsPerSecond,
                                                              bool loop,
                                                              float phaseOffset,
                                                              const VolumeDefaults& defaults) {
    SplinePathFollowerPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.splinePoints = std::move(splinePoints);
    primitive.speedUnitsPerSecond = ClampFiniteFloat(speedUnitsPerSecond, 2.0f, 0.0f, 128.0f);
    primitive.loop = loop;
    primitive.phaseOffset = ClampFiniteFloat(phaseOffset, 0.0f, -10.0f, 10.0f);
    return primitive;
}

CablePrimitive CreateCablePrimitive(const RuntimeVolumeSeed& data,
                                    const ri::math::Vec3& start,
                                    const ri::math::Vec3& end,
                                    float swayAmplitude,
                                    float swayFrequency,
                                    bool collisionEnabled,
                                    const VolumeDefaults& defaults) {
    CablePrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.start = SanitizeVector(start);
    primitive.end = SanitizeVector(end, {0.0f, -2.0f, 0.0f});
    primitive.swayAmplitude = ClampFiniteFloat(swayAmplitude, 0.12f, 0.0f, 4.0f);
    primitive.swayFrequency = ClampFiniteFloat(swayFrequency, 0.8f, 0.0f, 16.0f);
    primitive.collisionEnabled = collisionEnabled;
    return primitive;
}

ClippingRuntimeVolume CreateClippingRuntimeVolume(const RuntimeVolumeSeed& data,
                                                  std::vector<std::string> modes,
                                                  bool enabled,
                                                  const VolumeDefaults& defaults) {
    ClippingRuntimeVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    if (modes.empty()) {
        modes.emplace_back("visibility");
    }
    const ClipInteractionModeResolveResult resolved = ResolveClipInteractionModes(modes);
    volume.modes.clear();
    volume.modes.reserve(resolved.modes.size());
    for (const ClipVolumeMode mode : resolved.modes) {
        volume.modes.push_back(ToString(mode));
    }
    volume.enabled = enabled;
    return volume;
}

FilteredCollisionRuntimeVolume CreateFilteredCollisionRuntimeVolume(const RuntimeVolumeSeed& data,
                                                                    std::vector<std::string> channels,
                                                                    const VolumeDefaults& defaults) {
    FilteredCollisionRuntimeVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    if (channels.empty()) {
        channels.emplace_back("player");
    }
    volume.channels = std::move(channels);
    return volume;
}

KinematicTranslationPrimitive CreateKinematicTranslationPrimitive(const RuntimeVolumeSeed& data,
                                                                  const ri::math::Vec3& axis,
                                                                  float distance,
                                                                  float cycleSeconds,
                                                                  bool pingPong,
                                                                  const VolumeDefaults& defaults) {
    KinematicTranslationPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.axis = ri::math::LengthSquared(axis) <= 0.000001f
        ? ri::math::Vec3{1.0f, 0.0f, 0.0f}
        : ri::math::Normalize(SanitizeVector(axis, {1.0f, 0.0f, 0.0f}));
    primitive.distance = ClampFiniteFloat(distance, 2.0f, 0.0f, 1024.0f);
    primitive.cycleSeconds = ClampFiniteFloat(cycleSeconds, 3.0f, 0.1f, 120.0f);
    primitive.pingPong = pingPong;
    return primitive;
}

KinematicRotationPrimitive CreateKinematicRotationPrimitive(const RuntimeVolumeSeed& data,
                                                            const ri::math::Vec3& axis,
                                                            float angularSpeedDegreesPerSecond,
                                                            float maxAngleDegrees,
                                                            bool pingPong,
                                                            const VolumeDefaults& defaults) {
    KinematicRotationPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.axis = ri::math::LengthSquared(axis) <= 0.000001f
        ? ri::math::Vec3{0.0f, 1.0f, 0.0f}
        : ri::math::Normalize(SanitizeVector(axis, {0.0f, 1.0f, 0.0f}));
    primitive.angularSpeedDegreesPerSecond = ClampFiniteFloat(angularSpeedDegreesPerSecond, 45.0f, -1440.0f, 1440.0f);
    primitive.maxAngleDegrees = ClampFiniteFloat(maxAngleDegrees, 360.0f, 0.0f, 360.0f);
    primitive.pingPong = pingPong;
    return primitive;
}

TraversalLinkVolume CreateTraversalLinkVolume(const RuntimeVolumeSeed& data,
                                              TraversalLinkKind kind,
                                              float climbSpeed,
                                              const VolumeDefaults& defaults) {
    TraversalLinkVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.kind = kind;
    volume.type = data.type.empty() ? ToString(kind) : data.type;
    volume.climbSpeed = ClampFiniteFloat(climbSpeed, 3.4f, 0.4f, 12.0f);
    return volume;
}

PivotAnchorPrimitive CreatePivotAnchorPrimitive(const RuntimeVolumeSeed& data,
                                                std::string anchorId,
                                                const ri::math::Vec3& forwardAxis,
                                                bool alignToSurfaceNormal,
                                                const VolumeDefaults& defaults) {
    PivotAnchorPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.anchorId = anchorId.empty() ? primitive.id : std::move(anchorId);
    primitive.forwardAxis = SanitizeVector(forwardAxis, {0.0f, 0.0f, 1.0f});
    if (ri::math::LengthSquared(primitive.forwardAxis) <= 0.000001f) {
        primitive.forwardAxis = {0.0f, 0.0f, 1.0f};
    } else {
        primitive.forwardAxis = ri::math::Normalize(primitive.forwardAxis);
    }
    primitive.alignToSurfaceNormal = alignToSurfaceNormal;
    return primitive;
}

SymmetryMirrorPlane CreateSymmetryMirrorPlane(const RuntimeVolumeSeed& data,
                                              const ri::math::Vec3& planeNormal,
                                              float planeOffset,
                                              bool keepOriginal,
                                              bool snapToGrid,
                                              const VolumeDefaults& defaults) {
    SymmetryMirrorPlane helper{};
    static_cast<RuntimeVolume&>(helper) = CreateRuntimeVolume(data, defaults);
    helper.planeNormal = SanitizeVector(planeNormal, {1.0f, 0.0f, 0.0f});
    if (ri::math::LengthSquared(helper.planeNormal) <= 0.000001f) {
        helper.planeNormal = {1.0f, 0.0f, 0.0f};
    } else {
        helper.planeNormal = ri::math::Normalize(helper.planeNormal);
    }
    helper.planeOffset = ClampFiniteFloat(planeOffset, 0.0f, -100000.0f, 100000.0f);
    helper.keepOriginal = keepOriginal;
    helper.snapToGrid = snapToGrid;
    return helper;
}

LocalGridSnapVolume CreateLocalGridSnapVolume(const RuntimeVolumeSeed& data,
                                              float snapSize,
                                              bool snapX,
                                              bool snapY,
                                              bool snapZ,
                                              int priority,
                                              const VolumeDefaults& defaults) {
    LocalGridSnapVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.snapSize = ClampFiniteFloat(snapSize, 0.5f, 0.01f, 16.0f);
    volume.snapX = snapX;
    volume.snapY = snapY;
    volume.snapZ = snapZ;
    if (!volume.snapX && !volume.snapY && !volume.snapZ) {
        volume.snapX = true;
        volume.snapY = true;
        volume.snapZ = true;
    }
    volume.priority = std::clamp(priority, -1000, 1000);
    return volume;
}

HintPartitionVolume CreateHintPartitionVolume(const RuntimeVolumeSeed& data,
                                              HintPartitionMode mode,
                                              const VolumeDefaults& defaults) {
    HintPartitionVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.mode = mode;
    return volume;
}

DoorWindowCutoutPrimitive CreateDoorWindowCutoutPrimitive(const RuntimeVolumeSeed& data,
                                                          float openingWidth,
                                                          float openingHeight,
                                                          float sillHeight,
                                                          float lintelHeight,
                                                          bool carveCollision,
                                                          bool carveVisual,
                                                          const VolumeDefaults& defaults) {
    DoorWindowCutoutPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.openingWidth = ClampFiniteFloat(openingWidth, 2.0f, 0.1f, 64.0f);
    primitive.openingHeight = ClampFiniteFloat(openingHeight, 2.4f, 0.1f, 64.0f);
    primitive.sillHeight = ClampFiniteFloat(sillHeight, 0.0f, -16.0f, 32.0f);
    primitive.lintelHeight = ClampFiniteFloat(lintelHeight, primitive.openingHeight, -16.0f, 64.0f);
    if (primitive.lintelHeight < primitive.sillHeight) {
        std::swap(primitive.lintelHeight, primitive.sillHeight);
    }
    primitive.carveCollision = carveCollision;
    primitive.carveVisual = carveVisual;
    return primitive;
}

CameraConfinementVolume CreateCameraConfinementVolume(const RuntimeVolumeSeed& data,
                                                      const VolumeDefaults& defaults) {
    CameraConfinementVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    return volume;
}

LodOverrideVolume CreateLodOverrideVolume(const RuntimeVolumeSeed& data,
                                          std::vector<std::string> targetIds,
                                          ForcedLod forcedLod,
                                          const VolumeDefaults& defaults) {
    LodOverrideVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.targetIds = std::move(targetIds);
    volume.forcedLod = forcedLod;
    return volume;
}

LodSwitchPrimitive CreateLodSwitchPrimitive(const RuntimeVolumeSeed& data,
                                            std::vector<LodSwitchLevel> levels,
                                            const LodSwitchPolicy& policy,
                                            const LodSwitchDebugSettings& debug,
                                            const VolumeDefaults& defaults) {
    LodSwitchPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);

    if (levels.size() < 2U) {
        levels = {
            LodSwitchLevel{
                .name = "near",
                .representation = {},
                .collisionProfile = LodSwitchCollisionProfile::Full,
                .distanceEnter = 0.0f,
                .distanceExit = 28.0f,
            },
            LodSwitchLevel{
                .name = "far",
                .representation = {},
                .collisionProfile = LodSwitchCollisionProfile::Simplified,
                .distanceEnter = 24.0f,
                .distanceExit = 100000.0f,
            },
        };
    }

    for (LodSwitchLevel& level : levels) {
        level.name = level.name.empty() ? "lod_level" : level.name;
        level.distanceEnter = ClampFiniteFloat(level.distanceEnter, 0.0f, 0.0f, 1000000.0f);
        level.distanceExit = ClampFiniteFloat(level.distanceExit, level.distanceEnter, 0.0f, 1000000.0f);
        if (level.distanceExit < level.distanceEnter) {
            std::swap(level.distanceEnter, level.distanceExit);
        }
    }
    std::sort(levels.begin(), levels.end(), [](const LodSwitchLevel& lhs, const LodSwitchLevel& rhs) {
        return lhs.distanceEnter < rhs.distanceEnter;
    });

    primitive.levels = std::move(levels);
    primitive.policy.metric = policy.metric;
    primitive.policy.hysteresisEnabled = policy.hysteresisEnabled;
    primitive.policy.transitionMode = policy.transitionMode;
    primitive.policy.crossfadeSeconds = ClampFiniteFloat(policy.crossfadeSeconds, 0.0f, 0.0f, 8.0f);
    primitive.debug = debug;
    primitive.activeLevelIndex = 0U;
    primitive.previousLevelIndex = 0U;
    primitive.crossfadeAlpha = 1.0f;
    return primitive;
}

SurfaceScatterVolume CreateSurfaceScatterVolume(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    const SurfaceScatterSourceRepresentation& sourceRepresentation,
    const SurfaceScatterDensityControls& density,
    const SurfaceScatterDistributionControls& distribution,
    SurfaceScatterCollisionPolicy collisionPolicy,
    const SurfaceScatterCullingPolicy& culling,
    const SurfaceScatterAnimationSettings& animation,
    const VolumeDefaults& defaults) {
    SurfaceScatterVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.targetIds = std::move(targetIds);
    volume.targetIds.erase(
        std::remove_if(
            volume.targetIds.begin(),
            volume.targetIds.end(),
            [](const std::string& value) { return value.empty(); }),
        volume.targetIds.end());

    volume.sourceRepresentation = sourceRepresentation;
    volume.density.count = std::clamp<std::uint32_t>(density.count, 1U, 20000U);
    volume.density.densityPerSquareMeter = ClampFiniteFloat(density.densityPerSquareMeter, 0.0f, 0.0f, 500.0f);
    volume.density.maxPoints = std::clamp<std::uint32_t>(density.maxPoints, 1U, 20000U);
    if (volume.density.maxPoints < volume.density.count) {
        volume.density.maxPoints = volume.density.count;
    }

    volume.distribution.seed = distribution.seed;
    volume.distribution.minSlopeDegrees = ClampFiniteFloat(distribution.minSlopeDegrees, 0.0f, 0.0f, 90.0f);
    volume.distribution.maxSlopeDegrees = ClampFiniteFloat(distribution.maxSlopeDegrees, 85.0f, 0.0f, 90.0f);
    if (volume.distribution.maxSlopeDegrees < volume.distribution.minSlopeDegrees) {
        std::swap(volume.distribution.minSlopeDegrees, volume.distribution.maxSlopeDegrees);
    }
    volume.distribution.minHeight = ClampFiniteFloat(distribution.minHeight, -100000.0f, -1000000.0f, 1000000.0f);
    volume.distribution.maxHeight = ClampFiniteFloat(distribution.maxHeight, 100000.0f, -1000000.0f, 1000000.0f);
    if (volume.distribution.maxHeight < volume.distribution.minHeight) {
        std::swap(volume.distribution.minHeight, volume.distribution.maxHeight);
    }
    volume.distribution.minNormalY = ClampFiniteFloat(distribution.minNormalY, 0.0f, -1.0f, 1.0f);
    volume.distribution.minSeparation = ClampFiniteFloat(distribution.minSeparation, 0.0f, 0.0f, 1000.0f);
    volume.distribution.rotationJitterRadians = SanitizeVector(distribution.rotationJitterRadians);
    volume.distribution.scaleJitter = ri::math::Vec3{
        ClampFiniteFloat(distribution.scaleJitter.x, 0.0f, 0.0f, 8.0f),
        ClampFiniteFloat(distribution.scaleJitter.y, 0.0f, 0.0f, 8.0f),
        ClampFiniteFloat(distribution.scaleJitter.z, 0.0f, 0.0f, 8.0f),
    };
    volume.distribution.positionJitter = SanitizeVector(distribution.positionJitter);
    volume.collisionPolicy = collisionPolicy;
    volume.culling.maxActiveDistance = ClampFiniteFloat(culling.maxActiveDistance, 80.0f, 1.0f, 100000.0f);
    volume.culling.frustumCulling = culling.frustumCulling;
    volume.animation.windSwayEnabled = animation.windSwayEnabled;
    volume.animation.swayAmplitude = ClampFiniteFloat(animation.swayAmplitude, 0.0f, 0.0f, 10.0f);
    volume.animation.swayFrequency = ClampFiniteFloat(animation.swayFrequency, 0.0f, 0.0f, 20.0f);
    return volume;
}

SplineMeshDeformerPrimitive CreateSplineMeshDeformerPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    std::vector<ri::math::Vec3> splinePoints,
    std::uint32_t sampleCount,
    std::uint32_t sectionCount,
    float segmentLength,
    float tangentSmoothing,
    bool keepSource,
    bool collisionEnabled,
    bool navInfluence,
    bool dynamicEnabled,
    std::uint32_t seed,
    std::uint32_t maxSamples,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    SplineMeshDeformerPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.targetIds = std::move(targetIds);
    primitive.targetIds.erase(
        std::remove_if(
            primitive.targetIds.begin(),
            primitive.targetIds.end(),
            [](const std::string& value) { return value.empty(); }),
        primitive.targetIds.end());
    primitive.splinePoints = std::move(splinePoints);
    primitive.splinePoints.erase(
        std::remove_if(
            primitive.splinePoints.begin(),
            primitive.splinePoints.end(),
            [](const ri::math::Vec3& point) {
                return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z);
            }),
        primitive.splinePoints.end());
    primitive.sampleCount = std::clamp<std::uint32_t>(sampleCount, 2U, 2048U);
    primitive.sectionCount = std::clamp<std::uint32_t>(sectionCount, 1U, 256U);
    primitive.segmentLength = ClampFiniteFloat(segmentLength, 2.0f, 0.05f, 128.0f);
    primitive.tangentSmoothing = ClampFiniteFloat(tangentSmoothing, 0.5f, 0.0f, 1.0f);
    primitive.keepSource = keepSource;
    primitive.collisionEnabled = collisionEnabled;
    primitive.navInfluence = navInfluence;
    primitive.dynamicEnabled = dynamicEnabled;
    primitive.seed = seed;
    primitive.maxSamples = std::clamp<std::uint32_t>(maxSamples, 2U, 4096U);
    if (primitive.maxSamples < primitive.sampleCount) {
        primitive.maxSamples = primitive.sampleCount;
    }
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 120.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

SplineDecalRibbonPrimitive CreateSplineDecalRibbonPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<ri::math::Vec3> splinePoints,
    float width,
    std::uint32_t tessellation,
    float offsetY,
    float uvScaleU,
    float uvScaleV,
    float tangentSmoothing,
    bool transparentBlend,
    bool depthWrite,
    bool collisionEnabled,
    bool navInfluence,
    bool dynamicEnabled,
    std::uint32_t seed,
    std::uint32_t maxSamples,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    SplineDecalRibbonPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.splinePoints = std::move(splinePoints);
    primitive.splinePoints.erase(
        std::remove_if(
            primitive.splinePoints.begin(),
            primitive.splinePoints.end(),
            [](const ri::math::Vec3& point) {
                return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z);
            }),
        primitive.splinePoints.end());
    primitive.width = ClampFiniteFloat(width, 1.0f, 0.01f, 128.0f);
    primitive.tessellation = std::clamp<std::uint32_t>(tessellation, 2U, 4096U);
    primitive.offsetY = ClampFiniteFloat(offsetY, 0.03f, -10.0f, 10.0f);
    primitive.uvScaleU = ClampFiniteFloat(uvScaleU, 1.0f, 0.01f, 1000.0f);
    primitive.uvScaleV = ClampFiniteFloat(uvScaleV, 1.0f, 0.01f, 1000.0f);
    primitive.tangentSmoothing = ClampFiniteFloat(tangentSmoothing, 0.5f, 0.0f, 1.0f);
    primitive.transparentBlend = transparentBlend;
    primitive.depthWrite = depthWrite;
    primitive.collisionEnabled = collisionEnabled;
    primitive.navInfluence = navInfluence;
    primitive.dynamicEnabled = dynamicEnabled;
    primitive.seed = seed;
    primitive.maxSamples = std::clamp<std::uint32_t>(maxSamples, 2U, 4096U);
    if (primitive.maxSamples < primitive.tessellation) {
        primitive.maxSamples = primitive.tessellation;
    }
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 120.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

std::string SanitizeRemapMode(std::string_view value) {
    const std::string normalized = NormalizeToken(value);
    if (normalized == "triplanar" || normalized == "axis_dominant" || normalized == "planar_world") {
        return normalized;
    }
    return "triplanar";
}

TopologicalUvRemapperVolume CreateTopologicalUvRemapperVolume(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    std::string remapMode,
    std::string textureX,
    std::string textureY,
    std::string textureZ,
    std::string sharedTextureId,
    float projectionScale,
    float blendSharpness,
    const ri::math::Vec3& axisWeights,
    std::uint32_t maxMaterialPatches,
    float maxActiveDistance,
    bool frustumCulling,
    const ProceduralUvProjectionDebugControls& debug,
    const VolumeDefaults& defaults) {
    TopologicalUvRemapperVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.targetIds = std::move(targetIds);
    volume.targetIds.erase(
        std::remove_if(
            volume.targetIds.begin(),
            volume.targetIds.end(),
            [](const std::string& idValue) { return idValue.empty(); }),
        volume.targetIds.end());
    volume.remapMode = SanitizeRemapMode(remapMode);
    volume.textureX = std::move(textureX);
    volume.textureY = std::move(textureY);
    volume.textureZ = std::move(textureZ);
    volume.sharedTextureId = std::move(sharedTextureId);
    volume.projectionScale = ClampFiniteFloat(projectionScale, 1.0f, 1.0e-6f, 4096.0f);
    volume.blendSharpness = ClampFiniteFloat(blendSharpness, 4.0f, 0.25f, 64.0f);
    volume.axisWeights = ri::math::Vec3{
        ClampFiniteFloat(axisWeights.x, 1.0f, 0.001f, 8.0f),
        ClampFiniteFloat(axisWeights.y, 1.0f, 0.001f, 8.0f),
        ClampFiniteFloat(axisWeights.z, 1.0f, 0.001f, 8.0f),
    };
    volume.maxMaterialPatches = std::clamp<std::uint32_t>(maxMaterialPatches, 1U, 4096U);
    volume.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 512.0f, 1.0f, 100000.0f);
    volume.frustumCulling = frustumCulling;
    volume.debug = debug;
    return volume;
}

TriPlanarNode CreateTriPlanarNode(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    std::string textureX,
    std::string textureY,
    std::string textureZ,
    std::string sharedTextureId,
    float projectionScale,
    float blendSharpness,
    const ri::math::Vec3& axisWeights,
    std::uint32_t maxMaterialPatches,
    bool objectSpaceAxes,
    float maxActiveDistance,
    bool frustumCulling,
    const ProceduralUvProjectionDebugControls& debug,
    const VolumeDefaults& defaults) {
    TriPlanarNode node{};
    static_cast<RuntimeVolume&>(node) = CreateRuntimeVolume(data, defaults);
    node.targetIds = std::move(targetIds);
    node.targetIds.erase(
        std::remove_if(
            node.targetIds.begin(),
            node.targetIds.end(),
            [](const std::string& idValue) { return idValue.empty(); }),
        node.targetIds.end());
    node.textureX = std::move(textureX);
    node.textureY = std::move(textureY);
    node.textureZ = std::move(textureZ);
    node.sharedTextureId = std::move(sharedTextureId);
    node.projectionScale = ClampFiniteFloat(projectionScale, 1.0f, 1.0e-6f, 4096.0f);
    node.blendSharpness = ClampFiniteFloat(blendSharpness, 4.0f, 0.25f, 64.0f);
    node.axisWeights = ri::math::Vec3{
        ClampFiniteFloat(axisWeights.x, 1.0f, 0.001f, 8.0f),
        ClampFiniteFloat(axisWeights.y, 1.0f, 0.001f, 8.0f),
        ClampFiniteFloat(axisWeights.z, 1.0f, 0.001f, 8.0f),
    };
    node.maxMaterialPatches = std::clamp<std::uint32_t>(maxMaterialPatches, 1U, 4096U);
    node.objectSpaceAxes = objectSpaceAxes;
    node.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 512.0f, 1.0f, 100000.0f);
    node.frustumCulling = frustumCulling;
    node.debug = debug;
    return node;
}

InstanceCloudPrimitive CreateInstanceCloudPrimitive(
    const RuntimeVolumeSeed& data,
    const InstanceCloudSourceRepresentation& sourceRepresentation,
    std::uint32_t count,
    const ri::math::Vec3& offsetStep,
    const ri::math::Vec3& distributionExtents,
    std::uint32_t seed,
    const InstanceCloudVariationRanges& variation,
    InstanceCloudCollisionPolicy collisionPolicy,
    const InstanceCloudCullingPolicy& culling,
    const VolumeDefaults& defaults) {
    InstanceCloudPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.sourceRepresentation = sourceRepresentation;
    primitive.count = std::clamp<std::uint32_t>(count, 1U, 20000U);
    primitive.offsetStep = SanitizeVector(offsetStep);
    primitive.distributionExtents = ri::math::Vec3{
        ClampFiniteFloat(distributionExtents.x, 0.0f, 0.0f, 100000.0f),
        ClampFiniteFloat(distributionExtents.y, 0.0f, 0.0f, 100000.0f),
        ClampFiniteFloat(distributionExtents.z, 0.0f, 0.0f, 100000.0f),
    };
    primitive.seed = seed;
    primitive.variation.rotationJitterRadians = SanitizeVector(variation.rotationJitterRadians);
    primitive.variation.scaleJitter = ri::math::Vec3{
        ClampFiniteFloat(variation.scaleJitter.x, 0.0f, 0.0f, 8.0f),
        ClampFiniteFloat(variation.scaleJitter.y, 0.0f, 0.0f, 8.0f),
        ClampFiniteFloat(variation.scaleJitter.z, 0.0f, 0.0f, 8.0f),
    };
    primitive.variation.positionJitter = SanitizeVector(variation.positionJitter);
    primitive.collisionPolicy = collisionPolicy;
    primitive.culling.maxActiveDistance = ClampFiniteFloat(culling.maxActiveDistance, 80.0f, 1.0f, 100000.0f);
    primitive.culling.frustumCulling = culling.frustumCulling;
    return primitive;
}

VoronoiFracturePrimitive CreateVoronoiFracturePrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    std::uint32_t cellCount,
    float noiseJitter,
    std::uint32_t seed,
    bool capOpenFaces,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    VoronoiFracturePrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.targetIds = std::move(targetIds);
    primitive.targetIds.erase(
        std::remove_if(primitive.targetIds.begin(), primitive.targetIds.end(), [](const std::string& id) { return id.empty(); }),
        primitive.targetIds.end());
    primitive.cellCount = std::clamp<std::uint32_t>(cellCount, 2U, 1024U);
    primitive.noiseJitter = ClampFiniteFloat(noiseJitter, 0.1f, 0.0f, 1.0f);
    primitive.seed = seed;
    primitive.capOpenFaces = capOpenFaces;
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

MetaballPrimitive CreateMetaballPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<ri::math::Vec3> controlPoints,
    float isoLevel,
    float smoothing,
    std::uint32_t resolution,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    MetaballPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.controlPoints = std::move(controlPoints);
    primitive.controlPoints.erase(
        std::remove_if(
            primitive.controlPoints.begin(),
            primitive.controlPoints.end(),
            [](const ri::math::Vec3& point) {
                return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z);
            }),
        primitive.controlPoints.end());
    primitive.isoLevel = ClampFiniteFloat(isoLevel, 0.5f, 0.01f, 4.0f);
    primitive.smoothing = ClampFiniteFloat(smoothing, 0.35f, 0.0f, 2.0f);
    primitive.resolution = std::clamp<std::uint32_t>(resolution, 4U, 256U);
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

LatticeVolume CreateLatticeVolume(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    const ri::math::Vec3& cellSize,
    float beamRadius,
    std::uint32_t maxCells,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    LatticeVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.targetIds = std::move(targetIds);
    volume.targetIds.erase(
        std::remove_if(volume.targetIds.begin(), volume.targetIds.end(), [](const std::string& id) { return id.empty(); }),
        volume.targetIds.end());
    volume.cellSize = ri::math::Vec3{
        ClampFiniteFloat(cellSize.x, 0.5f, 0.01f, 64.0f),
        ClampFiniteFloat(cellSize.y, 0.5f, 0.01f, 64.0f),
        ClampFiniteFloat(cellSize.z, 0.5f, 0.01f, 64.0f),
    };
    volume.beamRadius = ClampFiniteFloat(beamRadius, 0.08f, 0.001f, 10.0f);
    volume.maxCells = std::clamp<std::uint32_t>(maxCells, 8U, 65536U);
    volume.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 120.0f, 1.0f, 100000.0f);
    volume.frustumCulling = frustumCulling;
    return volume;
}

ManifoldSweepPrimitive CreateManifoldSweepPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    std::vector<ri::math::Vec3> splinePoints,
    float profileRadius,
    std::uint32_t sampleCount,
    bool capEnds,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    ManifoldSweepPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.targetIds = std::move(targetIds);
    primitive.targetIds.erase(
        std::remove_if(primitive.targetIds.begin(), primitive.targetIds.end(), [](const std::string& id) { return id.empty(); }),
        primitive.targetIds.end());
    primitive.splinePoints = std::move(splinePoints);
    primitive.splinePoints.erase(
        std::remove_if(
            primitive.splinePoints.begin(),
            primitive.splinePoints.end(),
            [](const ri::math::Vec3& point) {
                return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z);
            }),
        primitive.splinePoints.end());
    primitive.profileRadius = ClampFiniteFloat(profileRadius, 0.25f, 0.001f, 16.0f);
    primitive.sampleCount = std::clamp<std::uint32_t>(sampleCount, 2U, 2048U);
    primitive.capEnds = capEnds;
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 128.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

TrimSheetSweepPrimitive CreateTrimSheetSweepPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    std::vector<ri::math::Vec3> splinePoints,
    std::string trimSheetId,
    float uvTileU,
    float uvTileV,
    std::uint32_t tessellation,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    TrimSheetSweepPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.targetIds = std::move(targetIds);
    primitive.targetIds.erase(
        std::remove_if(primitive.targetIds.begin(), primitive.targetIds.end(), [](const std::string& id) { return id.empty(); }),
        primitive.targetIds.end());
    primitive.splinePoints = std::move(splinePoints);
    primitive.splinePoints.erase(
        std::remove_if(
            primitive.splinePoints.begin(),
            primitive.splinePoints.end(),
            [](const ri::math::Vec3& point) {
                return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z);
            }),
        primitive.splinePoints.end());
    primitive.trimSheetId = std::move(trimSheetId);
    primitive.uvTileU = ClampFiniteFloat(uvTileU, 1.0f, 0.001f, 1024.0f);
    primitive.uvTileV = ClampFiniteFloat(uvTileV, 1.0f, 0.001f, 1024.0f);
    primitive.tessellation = std::clamp<std::uint32_t>(tessellation, 2U, 4096U);
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 128.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

LSystemBranchPrimitive CreateLSystemBranchPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    std::uint32_t iterations,
    float segmentLength,
    float branchAngleDegrees,
    std::uint32_t seed,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    LSystemBranchPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.targetIds = std::move(targetIds);
    primitive.targetIds.erase(
        std::remove_if(primitive.targetIds.begin(), primitive.targetIds.end(), [](const std::string& id) { return id.empty(); }),
        primitive.targetIds.end());
    primitive.iterations = std::clamp<std::uint32_t>(iterations, 1U, 10U);
    primitive.segmentLength = ClampFiniteFloat(segmentLength, 0.5f, 0.01f, 64.0f);
    primitive.branchAngleDegrees = ClampFiniteFloat(branchAngleDegrees, 22.5f, 0.0f, 180.0f);
    primitive.seed = seed;
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 120.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

GeodesicSpherePrimitive CreateGeodesicSpherePrimitive(
    const RuntimeVolumeSeed& data,
    std::uint32_t subdivisionLevel,
    float radiusScale,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    GeodesicSpherePrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.subdivisionLevel = std::clamp<std::uint32_t>(subdivisionLevel, 0U, 8U);
    primitive.radiusScale = ClampFiniteFloat(radiusScale, 1.0f, 0.001f, 1000.0f);
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 120.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

ExtrudeAlongNormalPrimitive CreateExtrudeAlongNormalPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    float distance,
    std::uint32_t shellCount,
    bool capOpenEdges,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    ExtrudeAlongNormalPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.targetIds = std::move(targetIds);
    primitive.targetIds.erase(
        std::remove_if(primitive.targetIds.begin(), primitive.targetIds.end(), [](const std::string& id) { return id.empty(); }),
        primitive.targetIds.end());
    primitive.distance = ClampFiniteFloat(distance, 0.2f, -100.0f, 100.0f);
    primitive.shellCount = std::clamp<std::uint32_t>(shellCount, 1U, 256U);
    primitive.capOpenEdges = capOpenEdges;
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 120.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

SuperellipsoidPrimitive CreateSuperellipsoidPrimitive(
    const RuntimeVolumeSeed& data,
    float exponentX,
    float exponentY,
    float exponentZ,
    std::uint32_t radialSegments,
    std::uint32_t rings,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    SuperellipsoidPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.exponentX = ClampFiniteFloat(exponentX, 2.0f, 0.1f, 16.0f);
    primitive.exponentY = ClampFiniteFloat(exponentY, 2.0f, 0.1f, 16.0f);
    primitive.exponentZ = ClampFiniteFloat(exponentZ, 2.0f, 0.1f, 16.0f);
    primitive.radialSegments = std::clamp<std::uint32_t>(radialSegments, 3U, 256U);
    primitive.rings = std::clamp<std::uint32_t>(rings, 2U, 256U);
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 120.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

PrimitiveDemoLattice CreatePrimitiveDemoLattice(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    const ri::math::Vec3& cellSize,
    std::uint32_t maxCells,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    PrimitiveDemoLattice primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.targetIds = std::move(targetIds);
    primitive.targetIds.erase(
        std::remove_if(primitive.targetIds.begin(), primitive.targetIds.end(), [](const std::string& id) { return id.empty(); }),
        primitive.targetIds.end());
    primitive.cellSize = ri::math::Vec3{
        ClampFiniteFloat(cellSize.x, 0.6f, 0.01f, 64.0f),
        ClampFiniteFloat(cellSize.y, 0.6f, 0.01f, 64.0f),
        ClampFiniteFloat(cellSize.z, 0.6f, 0.01f, 64.0f),
    };
    primitive.maxCells = std::clamp<std::uint32_t>(maxCells, 8U, 65536U);
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

PrimitiveDemoVoronoi CreatePrimitiveDemoVoronoi(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    std::uint32_t cellCount,
    float jitter,
    std::uint32_t seed,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    PrimitiveDemoVoronoi primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.targetIds = std::move(targetIds);
    primitive.targetIds.erase(
        std::remove_if(primitive.targetIds.begin(), primitive.targetIds.end(), [](const std::string& id) { return id.empty(); }),
        primitive.targetIds.end());
    primitive.cellCount = std::clamp<std::uint32_t>(cellCount, 2U, 1024U);
    primitive.jitter = ClampFiniteFloat(jitter, 0.1f, 0.0f, 1.0f);
    primitive.seed = seed;
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

ThickPolygonPrimitive CreateThickPolygonPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<ri::math::Vec3> points,
    float thickness,
    bool capTop,
    bool capBottom,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    ThickPolygonPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.points = std::move(points);
    primitive.points.erase(
        std::remove_if(
            primitive.points.begin(),
            primitive.points.end(),
            [](const ri::math::Vec3& point) {
                return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z);
            }),
        primitive.points.end());
    primitive.thickness = ClampFiniteFloat(thickness, 0.2f, 0.001f, 100.0f);
    primitive.capTop = capTop;
    primitive.capBottom = capBottom;
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

StructuralProfilePrimitive CreateStructuralProfilePrimitive(
    const RuntimeVolumeSeed& data,
    std::string profileId,
    float profileScale,
    std::uint32_t segmentCount,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    StructuralProfilePrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.profileId = std::move(profileId);
    primitive.profileScale = ClampFiniteFloat(profileScale, 1.0f, 0.001f, 1000.0f);
    primitive.segmentCount = std::clamp<std::uint32_t>(segmentCount, 2U, 4096U);
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

HalfPipePrimitive CreateHalfPipePrimitive(
    const RuntimeVolumeSeed& data,
    float radius,
    float length,
    std::uint32_t radialSegments,
    float wallThickness,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    HalfPipePrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.radius = ClampFiniteFloat(radius, 2.0f, 0.01f, 1000.0f);
    primitive.length = ClampFiniteFloat(length, 6.0f, 0.01f, 10000.0f);
    primitive.radialSegments = std::clamp<std::uint32_t>(radialSegments, 3U, 512U);
    primitive.wallThickness = ClampFiniteFloat(wallThickness, 0.2f, 0.001f, 100.0f);
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

QuarterPipePrimitive CreateQuarterPipePrimitive(
    const RuntimeVolumeSeed& data,
    float radius,
    float length,
    std::uint32_t radialSegments,
    float wallThickness,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    QuarterPipePrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.radius = ClampFiniteFloat(radius, 2.0f, 0.01f, 1000.0f);
    primitive.length = ClampFiniteFloat(length, 6.0f, 0.01f, 10000.0f);
    primitive.radialSegments = std::clamp<std::uint32_t>(radialSegments, 3U, 512U);
    primitive.wallThickness = ClampFiniteFloat(wallThickness, 0.2f, 0.001f, 100.0f);
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

PipeElbowPrimitive CreatePipeElbowPrimitive(
    const RuntimeVolumeSeed& data,
    float radius,
    float bendDegrees,
    std::uint32_t radialSegments,
    std::uint32_t bendSegments,
    float wallThickness,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    PipeElbowPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.radius = ClampFiniteFloat(radius, 1.0f, 0.01f, 1000.0f);
    primitive.bendDegrees = ClampFiniteFloat(bendDegrees, 90.0f, 1.0f, 359.0f);
    primitive.radialSegments = std::clamp<std::uint32_t>(radialSegments, 3U, 512U);
    primitive.bendSegments = std::clamp<std::uint32_t>(bendSegments, 1U, 512U);
    primitive.wallThickness = ClampFiniteFloat(wallThickness, 0.15f, 0.001f, 100.0f);
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

TorusSlicePrimitive CreateTorusSlicePrimitive(
    const RuntimeVolumeSeed& data,
    float majorRadius,
    float minorRadius,
    float sweepDegrees,
    std::uint32_t radialSegments,
    std::uint32_t tubularSegments,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    TorusSlicePrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.majorRadius = ClampFiniteFloat(majorRadius, 2.0f, 0.01f, 1000.0f);
    primitive.minorRadius = ClampFiniteFloat(minorRadius, 0.5f, 0.001f, 500.0f);
    primitive.sweepDegrees = ClampFiniteFloat(sweepDegrees, 180.0f, 1.0f, 360.0f);
    primitive.radialSegments = std::clamp<std::uint32_t>(radialSegments, 3U, 512U);
    primitive.tubularSegments = std::clamp<std::uint32_t>(tubularSegments, 3U, 512U);
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

SplineSweepPrimitive CreateSplineSweepPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds,
    std::vector<ri::math::Vec3> splinePoints,
    float profileRadius,
    std::uint32_t sampleCount,
    bool capEnds,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    SplineSweepPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.targetIds = std::move(targetIds);
    primitive.targetIds.erase(
        std::remove_if(primitive.targetIds.begin(), primitive.targetIds.end(), [](const std::string& id) { return id.empty(); }),
        primitive.targetIds.end());
    primitive.splinePoints = std::move(splinePoints);
    primitive.splinePoints.erase(
        std::remove_if(
            primitive.splinePoints.begin(),
            primitive.splinePoints.end(),
            [](const ri::math::Vec3& point) {
                return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z);
            }),
        primitive.splinePoints.end());
    primitive.profileRadius = ClampFiniteFloat(profileRadius, 0.25f, 0.001f, 16.0f);
    primitive.sampleCount = std::clamp<std::uint32_t>(sampleCount, 2U, 4096U);
    primitive.capEnds = capEnds;
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 120.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

RevolvePrimitive CreateRevolvePrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<ri::math::Vec3> profilePoints,
    float sweepDegrees,
    std::uint32_t segmentCount,
    bool capEnds,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    RevolvePrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.profilePoints = std::move(profilePoints);
    primitive.profilePoints.erase(
        std::remove_if(
            primitive.profilePoints.begin(),
            primitive.profilePoints.end(),
            [](const ri::math::Vec3& point) {
                return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z);
            }),
        primitive.profilePoints.end());
    primitive.sweepDegrees = ClampFiniteFloat(sweepDegrees, 360.0f, 1.0f, 360.0f);
    primitive.segmentCount = std::clamp<std::uint32_t>(segmentCount, 3U, 4096U);
    primitive.capEnds = capEnds;
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

DomeVaultPrimitive CreateDomeVaultPrimitive(
    const RuntimeVolumeSeed& data,
    float radius,
    float thickness,
    float heightRatio,
    std::uint32_t radialSegments,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    DomeVaultPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.radius = ClampFiniteFloat(radius, 4.0f, 0.01f, 1000.0f);
    primitive.thickness = ClampFiniteFloat(thickness, 0.25f, 0.001f, 100.0f);
    primitive.heightRatio = ClampFiniteFloat(heightRatio, 0.5f, 0.01f, 1.0f);
    primitive.radialSegments = std::clamp<std::uint32_t>(radialSegments, 3U, 1024U);
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 96.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

LoftPrimitive CreateLoftPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<ri::math::Vec3> pathPoints,
    std::vector<ri::math::Vec3> profilePoints,
    std::uint32_t segmentCount,
    bool capEnds,
    float maxActiveDistance,
    bool frustumCulling,
    const VolumeDefaults& defaults) {
    LoftPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.pathPoints = std::move(pathPoints);
    primitive.pathPoints.erase(
        std::remove_if(
            primitive.pathPoints.begin(),
            primitive.pathPoints.end(),
            [](const ri::math::Vec3& point) {
                return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z);
            }),
        primitive.pathPoints.end());
    primitive.profilePoints = std::move(profilePoints);
    primitive.profilePoints.erase(
        std::remove_if(
            primitive.profilePoints.begin(),
            primitive.profilePoints.end(),
            [](const ri::math::Vec3& point) {
                return !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z);
            }),
        primitive.profilePoints.end());
    primitive.segmentCount = std::clamp<std::uint32_t>(segmentCount, 2U, 4096U);
    primitive.capEnds = capEnds;
    primitive.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 120.0f, 1.0f, 100000.0f);
    primitive.frustumCulling = frustumCulling;
    return primitive;
}

NavmeshModifierVolume CreateNavmeshModifierVolume(const RuntimeVolumeSeed& data,
                                                  float traversalCost,
                                                  std::string_view tag,
                                                  const VolumeDefaults& defaults) {
    NavmeshModifierVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.traversalCost = ClampFiniteFloat(traversalCost, 1.5f, 0.01f, 100.0f);
    volume.tag = tag.empty() ? "modified" : std::string(tag);
    return volume;
}

VisibilityPrimitive CreateVisibilityPrimitive(const RuntimeVolumeSeed& data,
                                              VisibilityPrimitiveKind kind,
                                              const VolumeDefaults& defaults) {
    VisibilityPrimitive primitive{};
    primitive.id = data.id.empty() ? ri::runtime::CreateRuntimeId(defaults.runtimeId) : data.id;
    primitive.kind = kind;
    primitive.type = data.type.empty() ? ToString(kind) : data.type;
    primitive.debugVisible = data.debugVisible.value_or(true);
    primitive.position = data.position.value_or(ri::math::Vec3{0.0f, 0.0f, 0.0f});
    primitive.rotationRadians = data.rotationRadians.value_or(ri::math::Vec3{0.0f, 0.0f, 0.0f});

    const ri::math::Vec3 rawSize = data.size.value_or(defaults.size);
    primitive.size = ri::math::Vec3{
        PositiveExtent(rawSize.x),
        PositiveExtent(rawSize.y),
        PositiveExtent(rawSize.z),
    };
    return primitive;
}

ReflectionProbeVolume CreateReflectionProbeVolume(const RuntimeVolumeSeed& data,
                                                  float intensity,
                                                  float blendDistance,
                                                  std::uint32_t captureResolution,
                                                  bool boxProjection,
                                                  bool dynamicCapture,
                                                  const VolumeDefaults& defaults) {
    ReflectionProbeVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.intensity = ClampFiniteFloat(intensity, 1.0f, 0.0f, 8.0f);
    volume.blendDistance = ClampFiniteFloat(blendDistance, 1.5f, 0.0f, 64.0f);
    volume.captureResolution = std::clamp<std::uint32_t>(captureResolution, 64U, 2048U);
    volume.boxProjection = boxProjection;
    volume.dynamicCapture = dynamicCapture;
    return volume;
}

LightImportanceVolume CreateLightImportanceVolume(const RuntimeVolumeSeed& data,
                                                  bool probeGridBounds,
                                                  const VolumeDefaults& defaults) {
    LightImportanceVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.probeGridBounds = probeGridBounds;
    if (probeGridBounds) {
        volume.type = "probe_grid_bounds";
    }
    return volume;
}

LightPortalVolume CreateLightPortalVolume(const RuntimeVolumeSeed& data,
                                          float transmission,
                                          float softness,
                                          float priority,
                                          bool twoSided,
                                          const VolumeDefaults& defaults) {
    LightPortalVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.transmission = ClampFiniteFloat(transmission, 1.0f, 0.0f, 4.0f);
    volume.softness = ClampFiniteFloat(softness, 0.1f, 0.0f, 4.0f);
    volume.priority = ClampFiniteFloat(priority, 0.0f, -100.0f, 100.0f);
    volume.twoSided = twoSided;
    return volume;
}

VoxelGiBoundsVolume CreateVoxelGiBoundsVolume(const RuntimeVolumeSeed& data,
                                              float voxelSize,
                                              std::uint32_t cascadeCount,
                                              bool updateDynamics,
                                              const VolumeDefaults& defaults) {
    VoxelGiBoundsVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.voxelSize = ClampFiniteFloat(voxelSize, 1.0f, 0.05f, 16.0f);
    volume.cascadeCount = std::clamp<std::uint32_t>(cascadeCount, 1U, 8U);
    volume.updateDynamics = updateDynamics;
    return volume;
}

LightmapDensityVolume CreateLightmapDensityVolume(const RuntimeVolumeSeed& data,
                                                  float texelsPerMeter,
                                                  float minimumTexelsPerMeter,
                                                  float maximumTexelsPerMeter,
                                                  bool clampBySurfaceArea,
                                                  const VolumeDefaults& defaults) {
    LightmapDensityVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.texelsPerMeter = ClampFiniteFloat(texelsPerMeter, 256.0f, 4.0f, 4096.0f);
    volume.minimumTexelsPerMeter = ClampFiniteFloat(minimumTexelsPerMeter, 64.0f, 1.0f, 4096.0f);
    volume.maximumTexelsPerMeter = ClampFiniteFloat(maximumTexelsPerMeter, 1024.0f, 1.0f, 4096.0f);
    if (volume.maximumTexelsPerMeter < volume.minimumTexelsPerMeter) {
        std::swap(volume.minimumTexelsPerMeter, volume.maximumTexelsPerMeter);
    }
    volume.texelsPerMeter = std::clamp(volume.texelsPerMeter, volume.minimumTexelsPerMeter, volume.maximumTexelsPerMeter);
    volume.clampBySurfaceArea = clampBySurfaceArea;
    return volume;
}

ShadowExclusionVolume CreateShadowExclusionVolume(const RuntimeVolumeSeed& data,
                                                  bool excludeStaticShadows,
                                                  bool excludeDynamicShadows,
                                                  bool affectVolumetricShadows,
                                                  float fadeDistance,
                                                  const VolumeDefaults& defaults) {
    ShadowExclusionVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.excludeStaticShadows = excludeStaticShadows;
    volume.excludeDynamicShadows = excludeDynamicShadows;
    volume.affectVolumetricShadows = affectVolumetricShadows;
    volume.fadeDistance = ClampFiniteFloat(fadeDistance, 0.5f, 0.0f, 100.0f);
    return volume;
}

CullingDistanceVolume CreateCullingDistanceVolume(const RuntimeVolumeSeed& data,
                                                  float nearDistance,
                                                  float farDistance,
                                                  bool applyToStaticObjects,
                                                  bool applyToDynamicObjects,
                                                  bool allowHlod,
                                                  const VolumeDefaults& defaults) {
    CullingDistanceVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.nearDistance = ClampFiniteFloat(nearDistance, 0.0f, 0.0f, 100000.0f);
    volume.farDistance = ClampFiniteFloat(farDistance, 128.0f, 0.0f, 100000.0f);
    if (volume.farDistance < volume.nearDistance) {
        std::swap(volume.nearDistance, volume.farDistance);
    }
    volume.applyToStaticObjects = applyToStaticObjects;
    volume.applyToDynamicObjects = applyToDynamicObjects;
    volume.allowHlod = allowHlod;
    return volume;
}

ReferenceImagePlane CreateReferenceImagePlane(const RuntimeVolumeSeed& data,
                                              std::string textureId,
                                              std::string imageUrl,
                                              const ri::math::Vec3& tintColor,
                                              float opacity,
                                              int renderOrder,
                                              bool alwaysFaceCamera,
                                              const VolumeDefaults& defaults) {
    ReferenceImagePlane plane{};
    static_cast<RuntimeVolume&>(plane) = CreateRuntimeVolume(data, defaults);
    plane.textureId = std::move(textureId);
    plane.imageUrl = std::move(imageUrl);
    plane.tintColor = ri::math::Vec3{
        ClampFiniteFloat(tintColor.x, 1.0f, 0.0f, 1.0f),
        ClampFiniteFloat(tintColor.y, 1.0f, 0.0f, 1.0f),
        ClampFiniteFloat(tintColor.z, 1.0f, 0.0f, 1.0f),
    };
    plane.opacity = ClampFiniteFloat(opacity, 0.88f, 0.05f, 1.0f);
    plane.renderOrder = std::clamp(renderOrder, 1, 200);
    plane.alwaysFaceCamera = alwaysFaceCamera;
    return plane;
}

Text3dPrimitive CreateText3dPrimitive(const RuntimeVolumeSeed& data,
                                      std::string text,
                                      std::string fontFamily,
                                      std::string materialId,
                                      std::string textColor,
                                      std::string outlineColor,
                                      float textScale,
                                      float depth,
                                      float extrusionBevel,
                                      float letterSpacing,
                                      bool alwaysFaceCamera,
                                      bool doubleSided,
                                      const VolumeDefaults& defaults) {
    Text3dPrimitive textPrimitive{};
    static_cast<RuntimeVolume&>(textPrimitive) = CreateRuntimeVolume(data, defaults);
    textPrimitive.text = text.empty() ? "TEXT" : std::move(text);
    textPrimitive.fontFamily = fontFamily.empty() ? "default" : std::move(fontFamily);
    textPrimitive.materialId = std::move(materialId);
    textPrimitive.textColor = textColor.empty() ? "#ffffff" : std::move(textColor);
    textPrimitive.outlineColor = outlineColor.empty() ? "#000000" : std::move(outlineColor);
    textPrimitive.textScale = ClampFiniteFloat(textScale, 1.0f, 0.05f, 48.0f);
    textPrimitive.depth = ClampFiniteFloat(depth, 0.08f, 0.001f, 4.0f);
    textPrimitive.extrusionBevel = ClampFiniteFloat(extrusionBevel, 0.02f, 0.0f, 1.0f);
    textPrimitive.letterSpacing = ClampFiniteFloat(letterSpacing, 0.0f, -2.0f, 8.0f);
    textPrimitive.alwaysFaceCamera = alwaysFaceCamera;
    textPrimitive.doubleSided = doubleSided;
    return textPrimitive;
}

AnnotationCommentPrimitive CreateAnnotationCommentPrimitive(const RuntimeVolumeSeed& data,
                                                            std::string text,
                                                            std::string accentColor,
                                                            std::string backgroundColor,
                                                            float textScale,
                                                            float fontSize,
                                                            bool alwaysFaceCamera,
                                                            const VolumeDefaults& defaults) {
    AnnotationCommentPrimitive comment{};
    static_cast<RuntimeVolume&>(comment) = CreateRuntimeVolume(data, defaults);
    comment.text = text.empty() ? "NOTE" : std::move(text);
    comment.accentColor = accentColor.empty() ? "#ffd36a" : std::move(accentColor);
    comment.backgroundColor = backgroundColor.empty() ? "rgba(26, 22, 16, 0.88)" : std::move(backgroundColor);
    comment.textScale = ClampFiniteFloat(textScale, 2.4f, 0.2f, 20.0f);
    comment.fontSize = ClampFiniteFloat(fontSize, 24.0f, 8.0f, 128.0f);
    comment.alwaysFaceCamera = alwaysFaceCamera;
    return comment;
}

MeasureToolPrimitive CreateMeasureToolPrimitive(const RuntimeVolumeSeed& data,
                                                MeasureToolMode mode,
                                                const ri::math::Vec3& lineStart,
                                                const ri::math::Vec3& lineEnd,
                                                const ri::math::Vec3& labelOffset,
                                                std::string unitSuffix,
                                                std::string accentColor,
                                                std::string backgroundColor,
                                                std::string textColor,
                                                float textScale,
                                                float fontSize,
                                                bool showWireframe,
                                                bool showFill,
                                                bool alwaysFaceCamera,
                                                const VolumeDefaults& defaults) {
    MeasureToolPrimitive tool{};
    static_cast<RuntimeVolume&>(tool) = CreateRuntimeVolume(data, defaults);
    tool.mode = mode;
    tool.lineStart = SanitizeVector(lineStart);
    tool.lineEnd = SanitizeVector(lineEnd, {1.0f, 0.0f, 0.0f});
    if (ri::math::DistanceSquared(tool.lineStart, tool.lineEnd) <= 0.000001f) {
        tool.lineEnd = tool.lineStart + ri::math::Vec3{1.0f, 0.0f, 0.0f};
    }

    if (tool.mode == MeasureToolMode::Line) {
        const ri::math::Vec3 extents{
            std::max(0.1f, std::fabs(tool.lineEnd.x - tool.lineStart.x)),
            std::max(0.1f, std::fabs(tool.lineEnd.y - tool.lineStart.y)),
            std::max(0.1f, std::fabs(tool.lineEnd.z - tool.lineStart.z)),
        };
        tool.position = (tool.lineStart + tool.lineEnd) * 0.5f;
        tool.size = extents;
    }

    tool.labelOffset = SanitizeVector(labelOffset, {0.0f, 0.8f, 0.0f});
    tool.unitSuffix = unitSuffix.empty() ? "u" : std::move(unitSuffix);
    tool.accentColor = accentColor.empty() ? "#8cd8ff" : std::move(accentColor);
    tool.backgroundColor = backgroundColor.empty() ? "rgba(7, 18, 28, 0.82)" : std::move(backgroundColor);
    tool.textColor = textColor.empty() ? "#eaf8ff" : std::move(textColor);
    tool.textScale = ClampFiniteFloat(textScale, 3.2f, 0.5f, 24.0f);
    tool.fontSize = ClampFiniteFloat(fontSize, 34.0f, 14.0f, 128.0f);
    tool.showWireframe = showWireframe;
    tool.showFill = showFill;
    if (!tool.showWireframe && !tool.showFill) {
        tool.showWireframe = true;
    }
    tool.alwaysFaceCamera = alwaysFaceCamera;
    return tool;
}

RenderTargetSurface CreateRenderTargetSurface(const RuntimeVolumeSeed& data,
                                              const ri::math::Vec3& cameraPosition,
                                              const ri::math::Vec3& cameraLookAt,
                                              float cameraFovDegrees,
                                              int renderResolution,
                                              int resolutionCap,
                                              float maxActiveDistance,
                                              std::uint32_t updateEveryFrames,
                                              bool enableDistanceGate,
                                              bool editorOnly,
                                              const VolumeDefaults& defaults) {
    RenderTargetSurface surface{};
    static_cast<RuntimeVolume&>(surface) = CreateRuntimeVolume(data, defaults);
    surface.cameraPosition = SanitizeVector(cameraPosition, {0.0f, 2.0f, 0.0f});
    surface.cameraLookAt = SanitizeVector(cameraLookAt, {0.0f, 2.0f, -4.0f});
    surface.cameraFovDegrees = ClampFiniteFloat(cameraFovDegrees, 55.0f, 25.0f, 120.0f);
    surface.renderResolution = std::clamp(renderResolution, 64, 1024);
    surface.resolutionCap = std::clamp(resolutionCap, 64, 2048);
    surface.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 20.0f, 1.0f, 1000.0f);
    surface.updateEveryFrames = std::clamp<std::uint32_t>(updateEveryFrames, 1U, 120U);
    surface.enableDistanceGate = enableDistanceGate;
    surface.editorOnly = editorOnly;
    return surface;
}

PlanarReflectionSurface CreatePlanarReflectionSurface(const RuntimeVolumeSeed& data,
                                                      const ri::math::Vec3& planeNormal,
                                                      float reflectionStrength,
                                                      int renderResolution,
                                                      int resolutionCap,
                                                      float maxActiveDistance,
                                                      std::uint32_t updateEveryFrames,
                                                      bool enableDistanceGate,
                                                      bool editorOnly,
                                                      const VolumeDefaults& defaults) {
    PlanarReflectionSurface surface{};
    static_cast<RuntimeVolume&>(surface) = CreateRuntimeVolume(data, defaults);
    ri::math::Vec3 sanitizedNormal = SanitizeVector(planeNormal, {0.0f, 0.0f, 1.0f});
    const float normalLength = std::sqrt((sanitizedNormal.x * sanitizedNormal.x)
        + (sanitizedNormal.y * sanitizedNormal.y)
        + (sanitizedNormal.z * sanitizedNormal.z));
    if (normalLength <= 0.0001f) {
        sanitizedNormal = {0.0f, 0.0f, 1.0f};
    } else {
        sanitizedNormal = sanitizedNormal * (1.0f / normalLength);
    }
    surface.planeNormal = sanitizedNormal;
    surface.reflectionStrength = ClampFiniteFloat(reflectionStrength, 1.0f, 0.0f, 1.0f);
    surface.renderResolution = std::clamp(renderResolution, 64, 1024);
    surface.resolutionCap = std::clamp(resolutionCap, 64, 2048);
    surface.maxActiveDistance = ClampFiniteFloat(maxActiveDistance, 18.0f, 1.0f, 1000.0f);
    surface.updateEveryFrames = std::clamp<std::uint32_t>(updateEveryFrames, 1U, 120U);
    surface.enableDistanceGate = enableDistanceGate;
    surface.editorOnly = editorOnly;
    return surface;
}

PassThroughPrimitive CreatePassThroughPrimitive(const RuntimeVolumeSeed& data,
                                                PassThroughPrimitiveShape primitiveShape,
                                                std::string customMeshAsset,
                                                const PassThroughMaterialSettings& material,
                                                const PassThroughVisualBehavior& visualBehavior,
                                                const PassThroughInteractionProfile& interactionProfile,
                                                const PassThroughEventHooks& events,
                                                const PassThroughDebugSettings& debug,
                                                const VolumeDefaults& defaults) {
    PassThroughPrimitive primitive{};
    static_cast<RuntimeVolume&>(primitive) = CreateRuntimeVolume(data, defaults);
    primitive.primitiveShape = primitiveShape;
    primitive.customMeshAsset = std::move(customMeshAsset);
    if (primitive.primitiveShape == PassThroughPrimitiveShape::CustomMesh && primitive.customMeshAsset.empty()) {
        primitive.primitiveShape = PassThroughPrimitiveShape::Box;
    }

    primitive.material.baseColor = material.baseColor.empty() ? "#7fd6ff" : material.baseColor;
    primitive.material.opacity = ClampFiniteFloat(material.opacity, 0.35f, 0.0f, 1.0f);
    primitive.material.emissiveColor = material.emissiveColor.empty() ? "#7fd6ff" : material.emissiveColor;
    primitive.material.emissiveIntensity = ClampFiniteFloat(material.emissiveIntensity, 0.15f, 0.0f, 16.0f);
    primitive.material.doubleSided = material.doubleSided;
    primitive.material.depthWrite = material.depthWrite;
    primitive.material.depthTest = material.depthTest;
    primitive.material.blendMode = material.blendMode;

    primitive.visualBehavior.pulseEnabled = visualBehavior.pulseEnabled;
    primitive.visualBehavior.pulseSpeed = ClampFiniteFloat(visualBehavior.pulseSpeed, 1.2f, 0.0f, 24.0f);
    primitive.visualBehavior.pulseMinOpacity = ClampFiniteFloat(visualBehavior.pulseMinOpacity, 0.20f, 0.0f, 1.0f);
    primitive.visualBehavior.pulseMaxOpacity = ClampFiniteFloat(visualBehavior.pulseMaxOpacity, 0.45f, 0.0f, 1.0f);
    if (primitive.visualBehavior.pulseMinOpacity > primitive.visualBehavior.pulseMaxOpacity) {
        std::swap(primitive.visualBehavior.pulseMinOpacity, primitive.visualBehavior.pulseMaxOpacity);
    }
    primitive.visualBehavior.distanceFadeEnabled = visualBehavior.distanceFadeEnabled;
    primitive.visualBehavior.fadeNear = ClampFiniteFloat(visualBehavior.fadeNear, 1.0f, 0.0f, 10000.0f);
    primitive.visualBehavior.fadeFar = ClampFiniteFloat(visualBehavior.fadeFar, 20.0f, 0.0f, 10000.0f);
    if (primitive.visualBehavior.fadeFar < primitive.visualBehavior.fadeNear) {
        primitive.visualBehavior.fadeFar = primitive.visualBehavior.fadeNear;
    }
    primitive.visualBehavior.rimHighlightEnabled = visualBehavior.rimHighlightEnabled;
    primitive.visualBehavior.rimPower = ClampFiniteFloat(visualBehavior.rimPower, 2.0f, 0.1f, 16.0f);

    primitive.interactionProfile.blocksPlayer = interactionProfile.blocksPlayer;
    primitive.interactionProfile.blocksNpc = interactionProfile.blocksNpc;
    primitive.interactionProfile.blocksProjectiles = interactionProfile.blocksProjectiles;
    primitive.interactionProfile.affectsNavigation = interactionProfile.affectsNavigation;
    primitive.interactionProfile.raycastSelectable = interactionProfile.raycastSelectable;

    primitive.events.onEnter = events.onEnter;
    primitive.events.onExit = events.onExit;
    primitive.events.onUse = events.onUse;
    primitive.debug.label = debug.label;
    primitive.debug.showBounds = debug.showBounds;
    primitive.passThrough = !(primitive.interactionProfile.blocksPlayer
        || primitive.interactionProfile.blocksNpc
        || primitive.interactionProfile.blocksProjectiles);
    return primitive;
}

SkyProjectionSurface CreateSkyProjectionSurface(const RuntimeVolumeSeed& data,
                                                std::string primitiveType,
                                                const SkyProjectionVisualSettings& visual,
                                                const SkyProjectionBehaviorSettings& behavior,
                                                const SkyProjectionDebugSettings& debug,
                                                const VolumeDefaults& defaults) {
    SkyProjectionSurface surface{};
    static_cast<RuntimeVolume&>(surface) = CreateRuntimeVolume(data, defaults);
    const std::string normalizedPrimitiveType = NormalizeToken(primitiveType);
    if (normalizedPrimitiveType == "plane"
        || normalizedPrimitiveType == "box"
        || normalizedPrimitiveType == "cylinder"
        || normalizedPrimitiveType == "sphere"
        || normalizedPrimitiveType == "custom_mesh") {
        surface.primitiveType = normalizedPrimitiveType;
    } else {
        surface.primitiveType = "plane";
    }
    surface.visual.mode = visual.mode;
    surface.visual.color = visual.color.empty() ? "#8ab4ff" : visual.color;
    surface.visual.topColor = visual.topColor.empty() ? "#b8d4ff" : visual.topColor;
    surface.visual.bottomColor = visual.bottomColor.empty() ? "#6f94cc" : visual.bottomColor;
    surface.visual.textureId = visual.textureId;
    surface.visual.opacity = ClampFiniteFloat(visual.opacity, 1.0f, 0.0f, 1.0f);
    surface.visual.doubleSided = visual.doubleSided;
    surface.visual.unlit = visual.unlit;
    if (surface.visual.mode == SkyProjectionVisualMode::Texture && surface.visual.textureId.empty()) {
        surface.visual.mode = SkyProjectionVisualMode::Solid;
    }

    surface.behavior.followCameraYaw = behavior.followCameraYaw;
    surface.behavior.parallaxFactor = ClampFiniteFloat(behavior.parallaxFactor, 0.0f, 0.0f, 1.0f);
    surface.behavior.distanceLock = behavior.distanceLock;
    surface.behavior.depthWrite = behavior.depthWrite;
    surface.behavior.renderLayer = behavior.renderLayer;

    surface.debug.label = debug.label;
    surface.debug.showBounds = debug.showBounds;
    surface.skyProjectionSurface = true;
    return surface;
}

void SanitizeParticleSpawnAuthoringInPlace(ParticleSpawnAuthoring& spawn) {
    const float r = spawn.activation.outerProximityRadius;
    spawn.activation.outerProximityRadius =
        std::isfinite(r) ? std::clamp(r, 0.0f, 100000.0f) : 0.0f;
    spawn.emissionPolicy.burstCountOnEnter =
        std::min<std::uint32_t>(spawn.emissionPolicy.burstCountOnEnter, 100000U);
    if (spawn.budget.maxOnScreenCostHint > 1000000U) {
        spawn.budget.maxOnScreenCostHint = 1000000U;
    }
}

VolumetricEmitterBounds CreateVolumetricEmitterBounds(
    const RuntimeVolumeSeed& data,
    const VolumetricEmitterEmissionSettings& emission,
    const VolumetricEmitterParticleSettings& particle,
    const VolumetricEmitterRenderSettings& render,
    const VolumetricEmitterCullingSettings& culling,
    const VolumetricEmitterDebugSettings& debug,
    const VolumeDefaults& defaults,
    const std::optional<ParticleSpawnAuthoring>& particleSpawn) {
    VolumetricEmitterBounds volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);

    volume.emission.particleCount = std::clamp<std::uint32_t>(emission.particleCount, 8U, 2048U);
    volume.emission.spawnMode = emission.spawnMode;
    volume.emission.lifetimeMinSeconds = ClampFiniteFloat(emission.lifetimeMinSeconds, 2.0f, 0.01f, 120.0f);
    volume.emission.lifetimeMaxSeconds = ClampFiniteFloat(emission.lifetimeMaxSeconds, 6.0f, 0.01f, 120.0f);
    if (volume.emission.lifetimeMaxSeconds < volume.emission.lifetimeMinSeconds) {
        std::swap(volume.emission.lifetimeMinSeconds, volume.emission.lifetimeMaxSeconds);
    }
    volume.emission.spawnRatePerSecond = ClampFiniteFloat(emission.spawnRatePerSecond, 0.0f, 0.0f, 4096.0f);
    volume.emission.loop = emission.loop;

    volume.particle.size = ClampFiniteFloat(particle.size, 0.08f, 0.001f, 10.0f);
    volume.particle.sizeJitter = ClampFiniteFloat(particle.sizeJitter, 0.35f, 0.0f, 5.0f);
    volume.particle.color = particle.color.empty() ? "#d9dce4" : particle.color;
    volume.particle.opacity = ClampFiniteFloat(particle.opacity, 0.18f, 0.0f, 1.0f);
    volume.particle.velocity = SanitizeVector(particle.velocity, {0.0f, 0.04f, 0.0f});
    volume.particle.velocityJitter = SanitizeVector(particle.velocityJitter, {0.02f, 0.03f, 0.02f});
    volume.particle.softFade = particle.softFade;

    volume.render.blendMode = render.blendMode;
    volume.render.depthWrite = render.depthWrite;
    volume.render.depthTest = render.depthTest;
    volume.render.billboard = render.billboard;
    volume.render.sortMode = render.sortMode;

    volume.culling.maxActiveDistance = ClampFiniteFloat(culling.maxActiveDistance, 40.0f, 1.0f, 10000.0f);
    volume.culling.frustumCulling = culling.frustumCulling;
    volume.culling.pauseWhenOffscreen = culling.pauseWhenOffscreen;

    volume.debug.showBounds = debug.showBounds;
    volume.debug.showSpawnPoints = debug.showSpawnPoints;
    volume.debug.label = debug.label;
    if (particleSpawn.has_value()) {
        volume.particleSpawn = *particleSpawn;
        SanitizeParticleSpawnAuthoringInPlace(*volume.particleSpawn);
    }
    return volume;
}

OcclusionPortalVolume CreateOcclusionPortalVolume(const RuntimeVolumeSeed& data,
                                                  bool closed,
                                                  const VolumeDefaults& defaults) {
    OcclusionPortalVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.closed = closed;
    return volume;
}

PostProcessVolume CreatePostProcessVolume(const RuntimeVolumeSeed& data,
                                          float tintStrength,
                                          float blurAmount,
                                          float noiseAmount,
                                          float scanlineAmount,
                                          float barrelDistortion,
                                          float chromaticAberration,
                                          const VolumeDefaults& defaults) {
    PostProcessVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.tintStrength = ClampFiniteFloat(tintStrength, 0.12f, 0.0f, 1.0f);
    volume.blurAmount = ClampFiniteFloat(blurAmount, 0.0012f, 0.0f, 0.02f);
    volume.noiseAmount = ClampFiniteFloat(noiseAmount, 0.003f, 0.0f, 0.2f);
    volume.scanlineAmount = ClampFiniteFloat(scanlineAmount, 0.0015f, 0.0f, 0.08f);
    volume.barrelDistortion = ClampFiniteFloat(barrelDistortion, 0.003f, 0.0f, 0.1f);
    volume.chromaticAberration = ClampFiniteFloat(chromaticAberration, 0.00025f, 0.0f, 0.02f);
    return volume;
}

AudioReverbVolume CreateAudioReverbVolume(const RuntimeVolumeSeed& data,
                                          float reverbMix,
                                          float echoDelayMs,
                                          float echoFeedback,
                                          float dampening,
                                          float volumeScale,
                                          float playbackRate,
                                          const VolumeDefaults& defaults) {
    AudioReverbVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.reverbMix = ClampFiniteFloat(reverbMix, 0.55f, 0.0f, 1.0f);
    volume.echoDelayMs = ClampFiniteFloat(echoDelayMs, 160.0f, 0.0f, 2000.0f);
    volume.echoFeedback = ClampFiniteFloat(echoFeedback, 0.42f, 0.0f, 0.95f);
    volume.dampening = ClampFiniteFloat(dampening, 0.08f, 0.0f, 1.0f);
    volume.volumeScale = ClampFiniteFloat(volumeScale, 1.0f, 0.2f, 2.0f);
    volume.playbackRate = ClampFiniteFloat(playbackRate, 1.0f, 0.5f, 1.5f);
    return volume;
}

AudioOcclusionVolume CreateAudioOcclusionVolume(const RuntimeVolumeSeed& data,
                                                float occlusionStrength,
                                                float volumeScale,
                                                const VolumeDefaults& defaults) {
    AudioOcclusionVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.occlusionStrength = ClampFiniteFloat(occlusionStrength, 0.45f, 0.0f, 1.0f);
    volume.volumeScale = ClampFiniteFloat(volumeScale, 0.78f, 0.1f, 1.0f);
    return volume;
}

AmbientAudioVolume CreateAmbientAudioVolume(const RuntimeVolumeSeed& data,
                                            std::string audioPath,
                                            float baseVolume,
                                            float maxDistance,
                                            std::string label,
                                            std::vector<ri::math::Vec3> splinePoints,
                                            const VolumeDefaults& defaults) {
    AmbientAudioVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.audioPath = std::move(audioPath);
    volume.baseVolume = ClampFiniteFloat(baseVolume, 0.35f, 0.0f, 1.0f);
    const float fallbackDistance = std::max(volume.size.x, volume.size.z);
    volume.maxDistance = ClampFiniteFloat(maxDistance, fallbackDistance, 0.5f, 256.0f);
    volume.label = label.empty() ? std::string("ambient_audio") : std::move(label);
    volume.splinePoints = std::move(splinePoints);
    return volume;
}

GenericTriggerVolume CreateGenericTriggerVolume(const RuntimeVolumeSeed& data,
                                                double broadcastFrequency,
                                                const VolumeDefaults& defaults) {
    GenericTriggerVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    if (data.startArmed.has_value()) {
        volume.armed = *data.startArmed;
    }
    volume.broadcastFrequency = std::max(0.0, std::isfinite(broadcastFrequency) ? broadcastFrequency : 0.0);
    volume.nextBroadcastAt = 0.0;
    return volume;
}

SpatialQueryVolume CreateSpatialQueryVolume(const RuntimeVolumeSeed& data,
                                            double broadcastFrequency,
                                            std::uint32_t filterMask,
                                            const VolumeDefaults& defaults) {
    SpatialQueryVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.broadcastFrequency = std::max(0.0, std::isfinite(broadcastFrequency) ? broadcastFrequency : 0.0);
    volume.nextBroadcastAt = 0.0;
    volume.filterMask = filterMask;
    return volume;
}

StreamingLevelVolume CreateStreamingLevelVolume(const RuntimeVolumeSeed& data,
                                                std::string targetLevel,
                                                const VolumeDefaults& defaults) {
    StreamingLevelVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.targetLevel = std::move(targetLevel);
    return volume;
}

CheckpointSpawnVolume CreateCheckpointSpawnVolume(const RuntimeVolumeSeed& data,
                                                  std::string targetLevel,
                                                  const ri::math::Vec3& respawn,
                                                  const ri::math::Vec3& respawnRotation,
                                                  const VolumeDefaults& defaults) {
    CheckpointSpawnVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.targetLevel = std::move(targetLevel);
    volume.respawn = SanitizeVector(respawn, volume.position);
    volume.respawnRotation = SanitizeVector(respawnRotation);
    return volume;
}

TeleportVolume CreateTeleportVolume(const RuntimeVolumeSeed& data,
                                    std::string targetId,
                                    const ri::math::Vec3& targetPosition,
                                    const ri::math::Vec3& targetRotation,
                                    const ri::math::Vec3& offset,
                                    const VolumeDefaults& defaults) {
    TeleportVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.targetId = std::move(targetId);
    volume.targetPosition = SanitizeVector(targetPosition, volume.position);
    volume.targetRotation = SanitizeVector(targetRotation);
    volume.offset = SanitizeVector(offset);
    return volume;
}

LaunchVolume CreateLaunchVolume(const RuntimeVolumeSeed& data,
                                const ri::math::Vec3& impulse,
                                bool affectPhysics,
                                const VolumeDefaults& defaults) {
    LaunchVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.impulse = SanitizeVector(impulse, {0.0f, 8.0f, 0.0f});
    volume.affectPhysics = affectPhysics;
    return volume;
}

AnalyticsHeatmapVolume CreateAnalyticsHeatmapVolume(const RuntimeVolumeSeed& data,
                                                    const VolumeDefaults& defaults) {
    AnalyticsHeatmapVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    return volume;
}

LocalizedFogVolume CreateLocalizedFogVolume(const RuntimeVolumeSeed& data,
                                            float tintStrength,
                                            float blurAmount,
                                            const VolumeDefaults& defaults) {
    LocalizedFogVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.tintStrength = ClampFiniteFloat(tintStrength, 0.12f, 0.0f, 1.0f);
    volume.blurAmount = ClampFiniteFloat(blurAmount, 0.0016f, 0.0f, 0.02f);
    return volume;
}

FogBlockerVolume CreateFogBlockerVolume(const RuntimeVolumeSeed& data,
                                        const VolumeDefaults& defaults) {
    FogBlockerVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    return volume;
}

FluidSimulationVolume CreateFluidSimulationVolume(const RuntimeVolumeSeed& data,
                                                  float gravityScale,
                                                  float jumpScale,
                                                  float drag,
                                                  float buoyancy,
                                                  const ri::math::Vec3& flow,
                                                  float tintStrength,
                                                  float reverbMix,
                                                  float echoDelayMs,
                                                  const VolumeDefaults& defaults) {
    FluidSimulationVolume volume{};
    static_cast<RuntimeVolume&>(volume) = CreateRuntimeVolume(data, defaults);
    volume.gravityScale = ClampFiniteFloat(gravityScale, 0.35f, -2.0f, 4.0f);
    volume.jumpScale = ClampFiniteFloat(jumpScale, 0.72f, 0.0f, 4.0f);
    volume.drag = ClampFiniteFloat(drag, 1.8f, 0.0f, 8.0f);
    volume.buoyancy = ClampFiniteFloat(buoyancy, 0.9f, 0.0f, 3.0f);
    volume.flow = ri::math::Vec3{
        std::isfinite(flow.x) ? flow.x : 0.0f,
        std::isfinite(flow.y) ? flow.y : 0.0f,
        std::isfinite(flow.z) ? flow.z : 0.0f,
    };
    volume.tintStrength = ClampFiniteFloat(tintStrength, 0.22f, 0.0f, 1.0f);
    volume.reverbMix = ClampFiniteFloat(reverbMix, 0.35f, 0.0f, 1.0f);
    volume.echoDelayMs = ClampFiniteFloat(echoDelayMs, 120.0f, 0.0f, 2000.0f);
    return volume;
}

const CameraModifierVolume* GetActiveCameraModifierAt(const ri::math::Vec3& position,
                                                      const std::vector<CameraModifierVolume>& volumes) {
    const CameraModifierVolume* best = nullptr;
    for (const CameraModifierVolume& volume : volumes) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        if (best == nullptr || volume.priority >= best->priority) {
            best = &volume;
        }
    }
    return best;
}

CameraModifierBlendState BlendCameraModifierAt(const ri::math::Vec3& position,
                                               const std::vector<CameraModifierVolume>& volumes) {
    CameraModifierBlendState state{};
    float totalWeight = 0.0f;
    float fovAccumulator = 0.0f;
    float amplitudeAccumulator = 0.0f;
    float frequencyAccumulator = 0.0f;
    ri::math::Vec3 offsetAccumulator{};

    for (const CameraModifierVolume& volume : volumes) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        state.activeVolumeIds.push_back(volume.id);
        const float weight = std::max(0.0001f, std::max(0.0f, volume.priority + 101.0f));
        totalWeight += weight;
        fovAccumulator += volume.fov * weight;
        amplitudeAccumulator += volume.shakeAmplitude * weight;
        frequencyAccumulator += volume.shakeFrequency * weight;
        offsetAccumulator = offsetAccumulator + (volume.cameraOffset * weight);
    }

    if (totalWeight <= 0.0001f) {
        return state;
    }

    state.active = true;
    state.fov = ClampFiniteFloat(fovAccumulator / totalWeight, 58.0f, 20.0f, 140.0f);
    state.shakeAmplitude = ClampFiniteFloat(amplitudeAccumulator / totalWeight, 0.0f, 0.0f, 4.0f);
    state.shakeFrequency = ClampFiniteFloat(frequencyAccumulator / totalWeight, 0.0f, 0.0f, 64.0f);
    state.cameraOffset = SanitizeVector(offsetAccumulator / totalWeight, {});
    return state;
}

bool IsPositionInSafeZone(const ri::math::Vec3& position,
                          const std::vector<SafeZoneVolume>& volumes) {
    return std::any_of(volumes.begin(), volumes.end(), [&](const SafeZoneVolume& volume) {
        return IsPointInsideVolume(position, volume);
    });
}

} // namespace ri::world
