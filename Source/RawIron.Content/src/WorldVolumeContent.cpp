#include "RawIron/Content/WorldVolumeContent.h"

#include "RawIron/Content/PrefabExpansion.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ri::content {
namespace {

constexpr float kMinimumRuntimeExtent = 0.001f;

const Value* FindAny(const Value::Object& data, std::initializer_list<std::string_view> keys) {
    for (const std::string_view key : keys) {
        const auto it = data.find(key);
        if (it != data.end()) {
            return &it->second;
        }
    }
    return nullptr;
}

const Value* FindAnyInObject(const Value::Object* data, std::initializer_list<std::string_view> keys) {
    if (data == nullptr) {
        return nullptr;
    }
    return FindAny(*data, keys);
}

std::string_view GetStringOrEmpty(const Value* value) {
    if (value == nullptr) {
        return {};
    }

    const std::string* stringValue = value->TryGetString();
    return stringValue == nullptr ? std::string_view{} : std::string_view(*stringValue);
}

std::optional<double> ParseFiniteNumber(const Value* value) {
    if (value == nullptr) {
        return std::nullopt;
    }

    if (const double* number = value->TryGetNumber(); number != nullptr) {
        return std::isfinite(*number) ? std::optional<double>(*number) : std::nullopt;
    }

    const std::string* text = value->TryGetString();
    if (text == nullptr || text->empty()) {
        return std::nullopt;
    }

    double parsed = 0.0;
    const auto result = std::from_chars(text->data(), text->data() + text->size(), parsed);
    if (result.ec != std::errc() || !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed;
}

int HexNibble(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return 10 + (character - 'a');
    }
    if (character >= 'A' && character <= 'F') {
        return 10 + (character - 'A');
    }
    return -1;
}

std::optional<float> ParseHexChannel(char high, char low) {
    const int highNibble = HexNibble(high);
    const int lowNibble = HexNibble(low);
    if (highNibble < 0 || lowNibble < 0) {
        return std::nullopt;
    }
    return static_cast<float>((highNibble * 16) + lowNibble) / 255.0f;
}

bool ReadBoolean(const Value* value, bool fallback) {
    if (value == nullptr) {
        return fallback;
    }

    if (const bool* booleanValue = value->TryGetBoolean(); booleanValue != nullptr) {
        return *booleanValue;
    }

    if (const double* number = value->TryGetNumber(); number != nullptr && std::isfinite(*number)) {
        return *number != 0.0;
    }

    const std::string_view text = GetStringOrEmpty(value);
    if (text == "true" || text == "TRUE" || text == "1") {
        return true;
    }
    if (text == "false" || text == "FALSE" || text == "0") {
        return false;
    }

    return fallback;
}

std::vector<std::string> ParseStringList(const Value* value) {
    std::vector<std::string> parsed;
    if (value == nullptr) {
        return parsed;
    }

    if (const std::string* text = value->TryGetString(); text != nullptr) {
        parsed.push_back(*text);
        return parsed;
    }

    const Value::Array* array = value->TryGetArray();
    if (array == nullptr) {
        return parsed;
    }

    parsed.reserve(array->size());
    for (const Value& entry : *array) {
        if (const std::string* text = entry.TryGetString(); text != nullptr && !text->empty()) {
            parsed.push_back(*text);
        }
    }
    return parsed;
}

std::vector<ri::math::Vec3> ParseVec3List(const Value* value) {
    std::vector<ri::math::Vec3> parsed;
    const Value::Array* array = value == nullptr ? nullptr : value->TryGetArray();
    if (array == nullptr) {
        return parsed;
    }

    parsed.reserve(array->size());
    for (const Value& entry : *array) {
        parsed.push_back(SanitizeVec3(entry, {0.0f, 0.0f, 0.0f}));
    }
    return parsed;
}

std::optional<ri::world::VolumeShape> ParseVolumeShape(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "sphere") {
        return ri::world::VolumeShape::Sphere;
    }
    if (raw == "cylinder") {
        return ri::world::VolumeShape::Cylinder;
    }
    if (raw == "box") {
        return ri::world::VolumeShape::Box;
    }
    return std::nullopt;
}

std::optional<ri::world::PassThroughPrimitiveShape> ParsePassThroughPrimitiveShape(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "box") {
        return ri::world::PassThroughPrimitiveShape::Box;
    }
    if (raw == "plane") {
        return ri::world::PassThroughPrimitiveShape::Plane;
    }
    if (raw == "cylinder") {
        return ri::world::PassThroughPrimitiveShape::Cylinder;
    }
    if (raw == "sphere") {
        return ri::world::PassThroughPrimitiveShape::Sphere;
    }
    if (raw == "custom_mesh") {
        return ri::world::PassThroughPrimitiveShape::CustomMesh;
    }
    return std::nullopt;
}

std::optional<ri::world::PassThroughBlendMode> ParsePassThroughBlendMode(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "alpha") {
        return ri::world::PassThroughBlendMode::Alpha;
    }
    if (raw == "additive") {
        return ri::world::PassThroughBlendMode::Additive;
    }
    if (raw == "premultiplied") {
        return ri::world::PassThroughBlendMode::Premultiplied;
    }
    return std::nullopt;
}

std::optional<ri::world::SkyProjectionVisualMode> ParseSkyProjectionVisualMode(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "solid") {
        return ri::world::SkyProjectionVisualMode::Solid;
    }
    if (raw == "gradient") {
        return ri::world::SkyProjectionVisualMode::Gradient;
    }
    if (raw == "texture") {
        return ri::world::SkyProjectionVisualMode::Texture;
    }
    return std::nullopt;
}

std::optional<ri::world::SkyProjectionRenderLayer> ParseSkyProjectionRenderLayer(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "background") {
        return ri::world::SkyProjectionRenderLayer::Background;
    }
    if (raw == "world") {
        return ri::world::SkyProjectionRenderLayer::World;
    }
    if (raw == "foreground") {
        return ri::world::SkyProjectionRenderLayer::Foreground;
    }
    return std::nullopt;
}

std::optional<ri::world::VolumetricEmitterSpawnMode> ParseVolumetricEmitterSpawnMode(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "uniform") {
        return ri::world::VolumetricEmitterSpawnMode::Uniform;
    }
    if (raw == "surface") {
        return ri::world::VolumetricEmitterSpawnMode::Surface;
    }
    if (raw == "noise-clustered") {
        return ri::world::VolumetricEmitterSpawnMode::NoiseClustered;
    }
    return std::nullopt;
}

std::optional<ri::world::VolumetricEmitterBlendMode> ParseVolumetricEmitterBlendMode(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "alpha") {
        return ri::world::VolumetricEmitterBlendMode::Alpha;
    }
    if (raw == "additive") {
        return ri::world::VolumetricEmitterBlendMode::Additive;
    }
    return std::nullopt;
}

std::optional<ri::world::LodSwitchRepresentationKind> ParseLodSwitchRepresentationKind(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "primitive") {
        return ri::world::LodSwitchRepresentationKind::Primitive;
    }
    if (raw == "mesh") {
        return ri::world::LodSwitchRepresentationKind::Mesh;
    }
    if (raw == "cluster") {
        return ri::world::LodSwitchRepresentationKind::Cluster;
    }
    return std::nullopt;
}

std::optional<ri::world::LodSwitchCollisionProfile> ParseLodSwitchCollisionProfile(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "full") {
        return ri::world::LodSwitchCollisionProfile::Full;
    }
    if (raw == "simplified" || raw == "simplified_or_none") {
        return ri::world::LodSwitchCollisionProfile::Simplified;
    }
    if (raw == "none") {
        return ri::world::LodSwitchCollisionProfile::None;
    }
    return std::nullopt;
}

std::optional<ri::world::LodSwitchMetric> ParseLodSwitchMetric(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "camera_distance") {
        return ri::world::LodSwitchMetric::CameraDistance;
    }
    if (raw == "screen_size") {
        return ri::world::LodSwitchMetric::ScreenSize;
    }
    return std::nullopt;
}

std::optional<ri::world::LodSwitchTransitionMode> ParseLodSwitchTransitionMode(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "hard") {
        return ri::world::LodSwitchTransitionMode::Hard;
    }
    if (raw == "crossfade") {
        return ri::world::LodSwitchTransitionMode::Crossfade;
    }
    return std::nullopt;
}

std::optional<ri::world::InstanceCloudRepresentationKind> ParseInstanceCloudRepresentationKind(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "primitive") {
        return ri::world::InstanceCloudRepresentationKind::Primitive;
    }
    if (raw == "mesh") {
        return ri::world::InstanceCloudRepresentationKind::Mesh;
    }
    if (raw == "cluster") {
        return ri::world::InstanceCloudRepresentationKind::Cluster;
    }
    return std::nullopt;
}

std::optional<ri::world::InstanceCloudCollisionPolicy> ParseInstanceCloudCollisionPolicy(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "none") {
        return ri::world::InstanceCloudCollisionPolicy::None;
    }
    if (raw == "simplified") {
        return ri::world::InstanceCloudCollisionPolicy::Simplified;
    }
    if (raw == "per-instance" || raw == "per_instance") {
        return ri::world::InstanceCloudCollisionPolicy::PerInstance;
    }
    return std::nullopt;
}

std::optional<ri::world::SurfaceScatterRepresentationKind> ParseSurfaceScatterRepresentationKind(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "primitive") {
        return ri::world::SurfaceScatterRepresentationKind::Primitive;
    }
    if (raw == "mesh") {
        return ri::world::SurfaceScatterRepresentationKind::Mesh;
    }
    if (raw == "cluster") {
        return ri::world::SurfaceScatterRepresentationKind::Cluster;
    }
    return std::nullopt;
}

std::optional<ri::world::SurfaceScatterCollisionPolicy> ParseSurfaceScatterCollisionPolicy(const Value* value) {
    const std::string_view raw = GetStringOrEmpty(value);
    if (raw == "none") {
        return ri::world::SurfaceScatterCollisionPolicy::None;
    }
    if (raw == "proxy") {
        return ri::world::SurfaceScatterCollisionPolicy::Proxy;
    }
    if (raw == "full") {
        return ri::world::SurfaceScatterCollisionPolicy::Full;
    }
    return std::nullopt;
}

ri::world::RuntimeVolumeSeed BuildSeedWithSizeAliases(const Value::Object& data,
                                                      const ri::world::VolumeDefaults& defaults) {
    ri::world::RuntimeVolumeSeed seed{};
    seed.id = std::string(GetStringOrEmpty(FindAny(data, {"id"})));
    seed.type = std::string(GetStringOrEmpty(FindAny(data, {"type"})));
    if (const Value* debugVisible = FindAny(data, {"debugVisible", "showInEditor", "showDebug"}); debugVisible != nullptr) {
        seed.debugVisible = ReadBoolean(debugVisible, true);
    }
    seed.shape = ParseVolumeShape(FindAny(data, {"shape"}));

    if (const Value* position = FindAny(data, {"position"}); position != nullptr) {
        seed.position = SanitizeVec3(*position, {0.0f, 0.0f, 0.0f});
    }
    if (const Value* rotation = FindAny(data, {"rotation"}); rotation != nullptr) {
        seed.rotationRadians = SanitizeVec3(*rotation, {0.0f, 0.0f, 0.0f});
    }
    if (const Value* size = FindAny(data, {"size", "scale"}); size != nullptr) {
        const ri::math::Vec3 sanitizedSize = SanitizeVec3(*size, defaults.size);
        seed.size = ri::math::Vec3{
            std::fabs(sanitizedSize.x),
            std::fabs(sanitizedSize.y),
            std::fabs(sanitizedSize.z),
        };
    }
    if (const std::optional<double> radius = ParseFiniteNumber(FindAny(data, {"radius"})); radius.has_value()) {
        seed.radius = static_cast<float>(*radius);
    }
    if (const std::optional<double> height = ParseFiniteNumber(FindAny(data, {"height"})); height.has_value()) {
        seed.height = static_cast<float>(*height);
    }

    return seed;
}

double ReadClampedNumber(const Value::Object& data,
                         std::initializer_list<std::string_view> keys,
                         double fallback,
                         double minimum,
                         double maximum) {
    const Value* value = FindAny(data, keys);
    return value == nullptr ? fallback : ClampFiniteNumber(*value, fallback, minimum, maximum);
}

ri::math::Vec3 ReadVec3(const Value::Object& data,
                        std::initializer_list<std::string_view> keys,
                        const ri::math::Vec3& fallback = {0.0f, 0.0f, 0.0f}) {
    const Value* value = FindAny(data, keys);
    return value == nullptr ? fallback : SanitizeVec3(*value, fallback);
}

void ParseProceduralUvTextureAssignments(const Value::Object& data,
                                         std::string& textureX,
                                         std::string& textureY,
                                         std::string& textureZ,
                                         std::string& sharedTextureId) {
    textureX = std::string(GetStringOrEmpty(FindAny(data, {"textureX", "mapX", "texX"})));
    textureY = std::string(GetStringOrEmpty(FindAny(data, {"textureY", "mapY", "texY"})));
    textureZ = std::string(GetStringOrEmpty(FindAny(data, {"textureZ", "mapZ", "texZ"})));
    sharedTextureId = std::string(
        GetStringOrEmpty(FindAny(data, {"sharedTextureId", "sharedTexture", "textureId", "albedoMap"})));

    if (const Value* texturesValue = FindAny(data, {"textures", "textureSet", "maps"});
        texturesValue != nullptr) {
        if (const Value::Object* texturesObject = texturesValue->TryGetObject(); texturesObject != nullptr) {
            if (sharedTextureId.empty()) {
                sharedTextureId = std::string(
                    GetStringOrEmpty(FindAny(*texturesObject, {"shared", "albedo", "base", "map", "all"})));
            }
            if (textureX.empty()) {
                textureX = std::string(
                    GetStringOrEmpty(FindAny(*texturesObject, {"x", "positiveX", "u", "mapX", "texX"})));
            }
            if (textureY.empty()) {
                textureY = std::string(
                    GetStringOrEmpty(FindAny(*texturesObject, {"y", "positiveY", "v", "mapY", "texY"})));
            }
            if (textureZ.empty()) {
                textureZ = std::string(
                    GetStringOrEmpty(FindAny(*texturesObject, {"z", "positiveZ", "w", "mapZ", "texZ"})));
            }
        }
    }
}

ri::world::ProceduralUvProjectionDebugControls ParseProceduralUvDebugControls(const Value::Object& data) {
    ri::world::ProceduralUvProjectionDebugControls debug{};
    if (const Value* debugValue = FindAny(data, {"debug", "debugControls"}); debugValue != nullptr) {
        if (const Value::Object* debugObject = debugValue->TryGetObject(); debugObject != nullptr) {
            debug.previewTint = ReadBoolean(FindAny(*debugObject, {"previewTint", "tintPreview"}), false);
            debug.targetOutlines = ReadBoolean(FindAny(*debugObject, {"targetOutlines", "outlineTargets"}), false);
            debug.axisContributionPreview =
                ReadBoolean(FindAny(*debugObject, {"axisContributionPreview", "axisPreview", "showAxisContributions"}),
                            false);
            debug.texelDensityPreview =
                ReadBoolean(FindAny(*debugObject, {"texelDensityPreview", "densityPreview", "showTexelDensity"}), false);
            return debug;
        }
    }
    debug.previewTint = ReadBoolean(FindAny(data, {"previewTint", "tintPreview"}), false);
    debug.targetOutlines = ReadBoolean(FindAny(data, {"targetOutlines", "outlineTargets"}), false);
    debug.axisContributionPreview =
        ReadBoolean(FindAny(data, {"axisContributionPreview", "axisPreview", "showAxisContributions"}), false);
    debug.texelDensityPreview =
        ReadBoolean(FindAny(data, {"texelDensityPreview", "densityPreview", "showTexelDensity"}), false);
    return debug;
}

ri::math::Vec3 NormalizeOrFallback(const ri::math::Vec3& value, const ri::math::Vec3& fallback) {
    const ri::math::Vec3 normalized = ri::math::Normalize(value);
    if (ri::math::LengthSquared(normalized) <= 0.000001f) {
        return fallback;
    }
    return normalized;
}

ri::math::Vec3 ReadColor(const Value::Object& data,
                         std::initializer_list<std::string_view> keys,
                         const ri::math::Vec3& fallback) {
    const Value* value = FindAny(data, keys);
    if (value == nullptr) {
        return fallback;
    }

    if (const std::string* text = value->TryGetString(); text != nullptr) {
        if (text->size() == 7U && text->front() == '#') {
            const std::optional<float> r = ParseHexChannel(text->at(1), text->at(2));
            const std::optional<float> g = ParseHexChannel(text->at(3), text->at(4));
            const std::optional<float> b = ParseHexChannel(text->at(5), text->at(6));
            if (r.has_value() && g.has_value() && b.has_value()) {
                return ri::math::Vec3{*r, *g, *b};
            }
        }
        return fallback;
    }

    if (value->IsArray()) {
        const ri::math::Vec3 color = SanitizeVec3(*value, fallback);
        return ri::math::Vec3{
            std::clamp(color.x, 0.0f, 1.0f),
            std::clamp(color.y, 0.0f, 1.0f),
            std::clamp(color.z, 0.0f, 1.0f),
        };
    }

    return fallback;
}

void ClampMinimumExtents(ri::world::RuntimeVolumeSeed& seed, float minimumExtent = kMinimumRuntimeExtent) {
    if (!seed.size.has_value()) {
        return;
    }

    seed.size = ri::math::Vec3{
        std::max(minimumExtent, std::fabs(seed.size->x)),
        std::max(minimumExtent, std::fabs(seed.size->y)),
        std::max(minimumExtent, std::fabs(seed.size->z)),
    };
}

ri::world::RuntimeVolumeSeed BuildSeedForEnvironmentSphereVolume(const Value::Object& data,
                                                                 const ri::world::VolumeDefaults& defaults) {
    ri::world::RuntimeVolumeSeed seed = BuildSeedWithSizeAliases(data, defaults);
    if (!seed.shape.has_value()) {
        seed.shape = ri::world::VolumeShape::Sphere;
    }
    ClampMinimumExtents(seed);
    return seed;
}

ri::world::RuntimeVolumeSeed BuildSeedForFluidVolume(const Value::Object& data,
                                                     const ri::world::VolumeDefaults& defaults) {
    ri::world::RuntimeVolumeSeed seed = BuildSeedWithSizeAliases(data, defaults);
    if (!seed.shape.has_value() || seed.shape.value() == ri::world::VolumeShape::Sphere) {
        seed.shape = ri::world::VolumeShape::Box;
    }
    ClampMinimumExtents(seed);
    return seed;
}

ri::world::RuntimeVolumeSeed BuildSeedForPhysicsBoxVolume(const Value::Object& data,
                                                          const ri::world::VolumeDefaults& defaults) {
    ri::world::RuntimeVolumeSeed seed = BuildSeedWithSizeAliases(data, defaults);
    ClampMinimumExtents(seed);
    return seed;
}

} // namespace

ri::world::RuntimeVolumeSeed BuildRuntimeVolumeSeed(const Value::Object& data,
                                                    const ri::world::VolumeDefaults& defaults) {
    return BuildSeedWithSizeAliases(data, defaults);
}

ri::world::AuthoringRuntimeVolumeRecord BuildAuthoringRuntimeVolumeRecordFromLevelObject(
    const Value::Object& data,
    const ri::world::VolumeDefaults& defaults) {
    return ri::world::BuildAuthoringRuntimeVolumeRecord(BuildRuntimeVolumeSeed(data, defaults), defaults);
}

ri::world::FilteredCollisionVolume BuildFilteredCollisionVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "filtered_collision";
    defaults.type = "filtered_collision_volume";
    defaults.size = {2.0f, 2.0f, 2.0f};
    return ri::world::CreateFilteredCollisionVolume(
        BuildSeedWithSizeAliases(data, defaults),
        ParseStringList(FindAny(data, {"collisionChannels", "blocks"})),
        defaults);
}

ri::world::FilteredCollisionVolume BuildCameraBlockingVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "camera_blocking";
    defaults.type = "camera_blocking_volume";
    defaults.size = {2.0f, 2.0f, 2.0f};
    std::vector<std::string> channels = ParseStringList(FindAny(data, {"collisionChannels", "blocks"}));
    if (channels.empty()) {
        channels = {"camera"};
    }
    return ri::world::CreateFilteredCollisionVolume(
        BuildSeedWithSizeAliases(data, defaults),
        channels,
        defaults);
}

ri::world::ClipRuntimeVolume BuildAiPerceptionBlockerVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "ai_blocker";
    defaults.type = "ai_perception_blocker_volume";
    defaults.size = {2.0f, 2.0f, 2.0f};
    std::vector<std::string> modes = ParseStringList(FindAny(data, {"modes", "clipModes"}));
    if (modes.empty()) {
        modes = {"ai"};
    }
    return ri::world::CreateClipRuntimeVolume(
        BuildSeedWithSizeAliases(data, defaults),
        modes,
        ReadBoolean(FindAny(data, {"enabled"}), true),
        defaults);
}

ri::world::DamageVolume BuildDamageVolume(const Value::Object& data, bool killInstant) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = killInstant ? "kill_volume" : "damage_volume";
    defaults.type = killInstant ? "kill_volume" : "damage_volume";
    defaults.size = {4.0f, 4.0f, 4.0f};
    const double damagePerSecond = killInstant
        ? 9999.0
        : ReadClampedNumber(data, {"damagePerSecond", "damage"}, 18.0, 0.1, 5000.0);
    const bool explicitKill = ReadBoolean(FindAny(data, {"killInstant"}), false);
    const std::string label = std::string(GetStringOrEmpty(FindAny(data, {"label"})));
    return ri::world::CreateDamageVolume(
        BuildSeedWithSizeAliases(data, defaults),
        static_cast<float>(damagePerSecond),
        killInstant || explicitKill,
        label,
        defaults);
}

ri::world::CameraModifierVolume BuildCameraModifierVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "camera_modifier";
    defaults.type = "camera_modifier_volume";
    defaults.size = {5.0f, 5.0f, 5.0f};
    const double fov = ReadClampedNumber(data, {"fov", "fovOverride"}, 58.0, 20.0, 140.0);
    const double priority = ReadClampedNumber(data, {"priority"}, 0.0, -100.0, 100.0);
    const double blendDistance = ReadClampedNumber(data, {"blendDistance", "blendRadius"}, 2.0, 0.0, 64.0);
    const double shakeAmplitude = ReadClampedNumber(data, {"shakeAmplitude", "cameraShakeAmplitude"}, 0.0, 0.0, 4.0);
    const double shakeFrequency = ReadClampedNumber(data, {"shakeFrequency", "cameraShakeFrequency"}, 0.0, 0.0, 64.0);
    const ri::math::Vec3 cameraOffset = ReadVec3(data, {"cameraOffset", "offset"}, {0.0f, 0.0f, 0.0f});
    return ri::world::CreateCameraModifierVolume(
        BuildSeedWithSizeAliases(data, defaults),
        static_cast<float>(fov),
        static_cast<float>(priority),
        static_cast<float>(blendDistance),
        static_cast<float>(shakeAmplitude),
        static_cast<float>(shakeFrequency),
        cameraOffset,
        defaults);
}

ri::world::SafeZoneVolume BuildSafeZoneVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "safe_zone";
    defaults.type = "safe_zone_volume";
    defaults.size = {6.0f, 4.0f, 6.0f};
    const bool dropAggro = ReadBoolean(FindAny(data, {"dropAggro"}), true);
    return ri::world::CreateSafeZoneVolume(
        BuildSeedWithSizeAliases(data, defaults),
        dropAggro,
        defaults);
}

ri::world::PhysicsModifierVolume BuildPhysicsVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "physics";
    defaults.type = "physics_volume";
    defaults.size = {6.0f, 4.0f, 6.0f};

    const ri::math::Vec3 flowDirection = ReadVec3(
        data,
        {"flowDirection", "forceDirection", "currentDirection"},
        {0.0f, 0.0f, 0.0f});
    const float flowStrength = static_cast<float>(ReadClampedNumber(
        data,
        {"flowStrength", "force", "strength"},
        0.0,
        -30.0,
        30.0));
    return ri::world::CreateCustomGravityVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"gravityScale"}, 1.0, -2.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"jumpScale"}, 1.0, 0.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"drag"}, 0.0, 0.0, 8.0)),
        static_cast<float>(ReadClampedNumber(data, {"buoyancy"}, 0.0, 0.0, 3.0)),
        flowDirection * flowStrength,
        defaults);
}

ri::world::PhysicsModifierVolume BuildCustomGravityVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "custom_gravity";
    defaults.type = "custom_gravity_volume";
    defaults.size = {6.0f, 4.0f, 6.0f};

    ri::math::Vec3 flowDirection = ReadVec3(data, {"flowDirection", "forceDirection"}, {0.0f, 0.0f, 0.0f});
    float flowStrength = static_cast<float>(ReadClampedNumber(
        data,
        {"flowStrength", "force", "strength"},
        0.0,
        -30.0,
        30.0));
    if (ri::math::LengthSquared(flowDirection) <= 0.000001f) {
        flowDirection = ReadVec3(data, {"gravityDirection", "gravityVector"}, {0.0f, 0.0f, 0.0f});
        flowStrength = static_cast<float>(ReadClampedNumber(
            data,
            {"gravityStrength", "gravityForce"},
            static_cast<double>(flowStrength),
            -30.0,
            30.0));
    }

    return ri::world::CreateCustomGravityVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"gravityScale"}, 0.4, -2.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"jumpScale"}, 1.0, 0.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"drag"}, 0.0, 0.0, 8.0)),
        static_cast<float>(ReadClampedNumber(data, {"buoyancy"}, 0.0, 0.0, 3.0)),
        flowDirection * flowStrength,
        defaults);
}

ri::world::PhysicsModifierVolume BuildDirectionalWindVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "directional_wind";
    defaults.type = "directional_wind_volume";
    defaults.size = {6.0f, 4.0f, 6.0f};

    const ri::math::Vec3 direction = NormalizeOrFallback(
        ReadVec3(data, {"flowDirection", "forceDirection", "windDirection", "currentDirection"}, {1.0f, 0.0f, 0.0f}),
        {1.0f, 0.0f, 0.0f});
    const float strength = static_cast<float>(ReadClampedNumber(
        data,
        {"flowStrength", "force", "strength", "windStrength", "currentStrength"},
        4.5,
        -30.0,
        30.0));

    return ri::world::CreateDirectionalWindVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"drag"}, 0.4, 0.0, 8.0)),
        direction * strength,
        defaults);
}

ri::world::PhysicsModifierVolume BuildBuoyancyVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "buoyancy";
    defaults.type = "buoyancy_volume";
    defaults.size = {6.0f, 3.0f, 6.0f};

    const ri::math::Vec3 flowDirection = ReadVec3(
        data,
        {"flowDirection", "currentDirection", "forceDirection"},
        {0.0f, 0.0f, 0.0f});
    const float flowStrength = static_cast<float>(ReadClampedNumber(
        data,
        {"flowStrength", "currentStrength", "force", "strength"},
        0.0,
        -30.0,
        30.0));

    return ri::world::CreateBuoyancyVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"gravityScale"}, 0.8, -2.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"jumpScale"}, 0.9, 0.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"drag", "fluidDrag", "damping"}, 1.2, 0.0, 8.0)),
        static_cast<float>(ReadClampedNumber(data, {"buoyancy", "lift", "floatStrength"}, 0.85, 0.0, 3.0)),
        flowDirection * flowStrength,
        defaults);
}

ri::world::SurfaceVelocityPrimitive BuildSurfaceVelocityPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "surface_velocity";
    defaults.type = "surface_velocity_primitive";
    defaults.size = {4.0f, 0.6f, 4.0f};

    const ri::math::Vec3 direction = NormalizeOrFallback(
        ReadVec3(data, {"flowDirection", "forceDirection", "pointB"}, {1.0f, 0.0f, 0.0f}),
        {1.0f, 0.0f, 0.0f});
    const float strength = static_cast<float>(ReadClampedNumber(
        data,
        {"flowStrength", "force", "strength", "speed"},
        2.4,
        -30.0,
        30.0));

    return ri::world::CreateSurfaceVelocityPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        direction * strength,
        defaults);
}

ri::world::WaterSurfacePrimitive BuildWaterSurfacePrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "water_surface";
    defaults.type = "water_surface_primitive";
    defaults.size = {8.0f, 0.5f, 8.0f};
    return ri::world::CreateWaterSurfacePrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"waveAmplitude", "waveHeight"}, 0.08, 0.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"waveFrequency", "waveSpeed"}, 0.6, 0.0, 12.0)),
        static_cast<float>(ReadClampedNumber(data, {"flowSpeed", "currentSpeed"}, 0.0, -30.0, 30.0)),
        ReadBoolean(FindAny(data, {"blocksUnderwaterFog", "blockUnderwaterFog"}), false),
        defaults);
}

ri::world::RadialForceVolume BuildRadialForceVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "radial_force";
    defaults.type = "radial_force_volume";
    defaults.size = {6.0f, 6.0f, 6.0f};
    const std::string_view mode = GetStringOrEmpty(FindAny(data, {"mode", "forceMode"}));
    const float authoredStrength = static_cast<float>(ReadClampedNumber(
        data,
        {"strength", "force", "flowStrength"},
        4.2,
        -40.0,
        40.0));
    const float signedStrength = (mode == "inward" || mode == "attractor" || mode == "pull")
        ? -std::fabs(authoredStrength)
        : authoredStrength;
    return ri::world::CreateRadialForceVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        signedStrength,
        static_cast<float>(ReadClampedNumber(data, {"falloff"}, 1.0, 0.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"innerRadius", "deadzone", "deadZone"}, 0.0, 0.0, 256.0)),
        defaults);
}

ri::world::PhysicsConstraintVolume BuildPhysicsConstraintVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "physics_constraint";
    defaults.type = "physics_constraint_volume";
    defaults.size = {6.0f, 4.0f, 6.0f};
    std::vector<std::string> lockAxes = ParseStringList(FindAny(data, {"lockAxes", "lockedAxes"}));
    const std::string_view lockPlane = GetStringOrEmpty(FindAny(data, {"lockPlane", "plane"}));
    if (lockPlane == "xy" || lockPlane == "yx") {
        lockAxes.push_back("x");
        lockAxes.push_back("y");
    } else if (lockPlane == "xz" || lockPlane == "zx") {
        lockAxes.push_back("x");
        lockAxes.push_back("z");
    } else if (lockPlane == "yz" || lockPlane == "zy") {
        lockAxes.push_back("y");
        lockAxes.push_back("z");
    }
    if (ReadBoolean(FindAny(data, {"lockAll", "freeze"}), false)) {
        lockAxes = {"x", "y", "z"};
    }
    return ri::world::CreatePhysicsConstraintVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        lockAxes,
        defaults);
}

ri::world::SplinePathFollowerPrimitive BuildSplinePathFollowerPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "spline_path_follower";
    defaults.type = "spline_path_follower_primitive";
    defaults.size = {2.0f, 2.0f, 2.0f};
    std::vector<ri::math::Vec3> splinePoints = ParseVec3List(FindAny(data, {"spline", "path", "points"}));
    if (splinePoints.empty()) {
        const ri::math::Vec3 position = ReadVec3(data, {"position"}, {0.0f, 0.0f, 0.0f});
        splinePoints.push_back(position);
    }
    return ri::world::CreateSplinePathFollowerPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        std::move(splinePoints),
        static_cast<float>(ReadClampedNumber(data, {"speed", "speedUnitsPerSecond"}, 2.0, 0.0, 128.0)),
        ReadBoolean(FindAny(data, {"loop"}), true),
        static_cast<float>(ReadClampedNumber(data, {"phaseOffset", "offset"}, 0.0, -10.0, 10.0)),
        defaults);
}

ri::world::CablePrimitive BuildCablePrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "cable";
    defaults.type = "cable_primitive";
    defaults.size = {1.0f, 2.0f, 1.0f};
    const ri::math::Vec3 start = ReadVec3(data, {"start", "pointA"}, {0.0f, 0.0f, 0.0f});
    const ri::math::Vec3 end = ReadVec3(data, {"end", "pointB"}, {0.0f, -2.0f, 0.0f});
    return ri::world::CreateCablePrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        start,
        end,
        static_cast<float>(ReadClampedNumber(data, {"swayAmplitude", "amplitude"}, 0.12, 0.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"swayFrequency", "frequency"}, 0.8, 0.0, 16.0)),
        ReadBoolean(FindAny(data, {"collisionEnabled", "collides"}), false),
        defaults);
}

ri::world::ClippingRuntimeVolume BuildClippingVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "clipping";
    defaults.type = "clipping_volume";
    defaults.size = {4.0f, 4.0f, 4.0f};
    return ri::world::CreateClippingRuntimeVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"modes", "clipModes"})),
        ReadBoolean(FindAny(data, {"enabled"}), true),
        defaults);
}

ri::world::FilteredCollisionRuntimeVolume BuildFilteredCollisionRuntimeVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "filtered_collision_runtime";
    defaults.type = "filtered_collision_volume";
    defaults.size = {2.0f, 2.0f, 2.0f};
    return ri::world::CreateFilteredCollisionRuntimeVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"collisionChannels", "channels", "blocks"})),
        defaults);
}

ri::world::KinematicTranslationPrimitive BuildKinematicTranslationPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "kinematic_translation";
    defaults.type = "kinematic_translation_primitive";
    defaults.size = {2.0f, 1.0f, 2.0f};
    const ri::math::Vec3 axis = NormalizeOrFallback(
        ReadVec3(data, {"axis", "direction", "translationAxis"}, {1.0f, 0.0f, 0.0f}),
        {1.0f, 0.0f, 0.0f});
    return ri::world::CreateKinematicTranslationPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        axis,
        static_cast<float>(ReadClampedNumber(data, {"distance", "amplitude", "range"}, 2.0, 0.0, 1024.0)),
        static_cast<float>(ReadClampedNumber(data, {"cycleSeconds", "period", "speedSeconds"}, 3.0, 0.1, 120.0)),
        ReadBoolean(FindAny(data, {"pingPong"}), true),
        defaults);
}

ri::world::KinematicRotationPrimitive BuildKinematicRotationPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "kinematic_rotation";
    defaults.type = "kinematic_rotation_primitive";
    defaults.size = {2.0f, 2.0f, 2.0f};
    const ri::math::Vec3 axis = NormalizeOrFallback(
        ReadVec3(data, {"axis", "rotationAxis"}, {0.0f, 1.0f, 0.0f}),
        {0.0f, 1.0f, 0.0f});
    return ri::world::CreateKinematicRotationPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        axis,
        static_cast<float>(ReadClampedNumber(data, {"angularSpeed", "degreesPerSecond", "speed"}, 45.0, -1440.0, 1440.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxAngleDegrees", "angleLimit"}, 360.0, 0.0, 360.0)),
        ReadBoolean(FindAny(data, {"pingPong"}), false),
        defaults);
}

ri::world::TraversalLinkVolume BuildTraversalLinkVolume(const Value::Object& data) {
    const std::string_view rawType = GetStringOrEmpty(FindAny(data, {"type"}));
    ri::world::TraversalLinkKind kind = ri::world::TraversalLinkKind::General;
    std::string runtimeId = "traversal_link";
    std::string type = "traversal_link_volume";
    if (rawType == "ladder_volume") {
        kind = ri::world::TraversalLinkKind::Ladder;
        runtimeId = "ladder";
        type = "ladder_volume";
    } else if (rawType == "climb_volume") {
        kind = ri::world::TraversalLinkKind::Climb;
        runtimeId = "climb";
        type = "climb_volume";
    }

    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = runtimeId;
    defaults.type = type;
    defaults.size = {2.0f, 4.0f, 2.0f};
    return ri::world::CreateTraversalLinkVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        kind,
        static_cast<float>(ReadClampedNumber(data, {"climbSpeed"}, 3.4, 0.4, 12.0)),
        defaults);
}

ri::world::PivotAnchorPrimitive BuildPivotAnchorPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "pivot_anchor";
    defaults.type = "pivot_anchor_primitive";
    defaults.size = {1.0f, 1.0f, 1.0f};
    return ri::world::CreatePivotAnchorPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        std::string(GetStringOrEmpty(FindAny(data, {"anchorId", "pivotId", "targetId"}))),
        ReadVec3(data, {"forwardAxis", "axis", "normal"}, {0.0f, 0.0f, 1.0f}),
        ReadBoolean(FindAny(data, {"alignToSurfaceNormal", "alignNormal"}), false),
        defaults);
}

ri::world::SymmetryMirrorPlane BuildSymmetryMirrorPlane(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "symmetry_mirror";
    defaults.type = "symmetry_mirror_plane";
    defaults.size = {8.0f, 8.0f, 0.25f};
    return ri::world::CreateSymmetryMirrorPlane(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ReadVec3(data, {"planeNormal", "normal", "mirrorNormal"}, {1.0f, 0.0f, 0.0f}),
        static_cast<float>(ReadClampedNumber(data, {"planeOffset", "offset"}, 0.0, -100000.0, 100000.0)),
        ReadBoolean(FindAny(data, {"keepOriginal"}), true),
        ReadBoolean(FindAny(data, {"snapToGrid"}), false),
        defaults);
}

ri::world::LocalGridSnapVolume BuildLocalGridSnapVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "local_grid_snap";
    defaults.type = "local_grid_snap_volume";
    defaults.size = {6.0f, 4.0f, 6.0f};

    bool snapX = ReadBoolean(FindAny(data, {"snapX"}), true);
    bool snapY = ReadBoolean(FindAny(data, {"snapY"}), true);
    bool snapZ = ReadBoolean(FindAny(data, {"snapZ"}), true);
    const std::vector<std::string> snapAxes = ParseStringList(FindAny(data, {"snapAxes", "axes"}));
    if (!snapAxes.empty()) {
        snapX = false;
        snapY = false;
        snapZ = false;
        for (const std::string& axis : snapAxes) {
            if (axis == "x" || axis == "X") {
                snapX = true;
            } else if (axis == "y" || axis == "Y") {
                snapY = true;
            } else if (axis == "z" || axis == "Z") {
                snapZ = true;
            }
        }
    }
    if (!snapX && !snapY && !snapZ) {
        snapX = true;
        snapY = true;
        snapZ = true;
    }

    int priority = 0;
    if (const Value* rawPriority = FindAny(data, {"priority"}); rawPriority != nullptr) {
        priority = ClampFiniteInteger(*rawPriority, 0, -1000, 1000);
    }

    return ri::world::CreateLocalGridSnapVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"snapSize", "gridSize"}, 0.5, 0.01, 16.0)),
        snapX,
        snapY,
        snapZ,
        priority,
        defaults);
}

ri::world::HintPartitionVolume BuildHintPartitionVolume(const Value::Object& data) {
    const std::string_view rawMode = GetStringOrEmpty(FindAny(data, {"hintMode", "partitionMode"}));
    const bool skip = rawMode == "skip";

    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "hint_skip";
    defaults.type = "hint_skip_brush";
    defaults.size = {6.0f, 4.0f, 6.0f};
    return ri::world::CreateHintPartitionVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        skip ? ri::world::HintPartitionMode::Skip : ri::world::HintPartitionMode::Hint,
        defaults);
}

ri::world::DoorWindowCutoutPrimitive BuildDoorWindowCutoutPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "door_window_cutout";
    defaults.type = "door_window_cutout";
    defaults.size = {4.0f, 4.0f, 1.0f};
    return ri::world::CreateDoorWindowCutoutPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"openingWidth", "doorWidth", "windowWidth"}, 2.0, 0.1, 64.0)),
        static_cast<float>(ReadClampedNumber(data, {"openingHeight", "doorHeight", "windowHeight"}, 2.4, 0.1, 64.0)),
        static_cast<float>(ReadClampedNumber(data, {"sillHeight"}, 0.0, -16.0, 32.0)),
        static_cast<float>(ReadClampedNumber(data, {"lintelHeight", "headerHeight"}, 2.4, -16.0, 64.0)),
        ReadBoolean(FindAny(data, {"carveCollision"}), true),
        ReadBoolean(FindAny(data, {"carveVisual"}), true),
        defaults);
}

ri::world::ProceduralDoorEntity BuildProceduralDoorEntity(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "procedural_door";
    defaults.type = "procedural_door";
    defaults.size = {1.4f, 2.4f, 0.3f};

    const ri::world::RuntimeVolumeSeed seed = BuildSeedForPhysicsBoxVolume(data, defaults);
    ri::world::ProceduralDoorEntity door{};
    static_cast<ri::world::RuntimeVolume&>(door) = ri::world::CreateRuntimeVolume(seed, defaults);
    door.openingWidth = static_cast<float>(ReadClampedNumber(data, {"openingWidth", "doorWidth", "width"}, 1.2, 0.2, 16.0));
    door.openingHeight = static_cast<float>(ReadClampedNumber(data, {"openingHeight", "doorHeight", "height"}, 2.2, 0.2, 16.0));
    door.thickness = static_cast<float>(ReadClampedNumber(data, {"thickness", "depth"}, 0.2, 0.01, 4.0));
    door.startsOpen = ReadBoolean(FindAny(data, {"startsOpen", "openByDefault"}), false);
    door.startsLocked = ReadBoolean(FindAny(data, {"startsLocked", "locked"}), false);
    door.blocksWhileClosed = ReadBoolean(FindAny(data, {"blocksWhileClosed", "collidableWhenClosed"}), true);
    door.interactionPrompt = std::string(GetStringOrEmpty(FindAny(data, {"interactionPrompt", "prompt", "usePrompt"})));
    if (door.interactionPrompt.empty()) {
        door.interactionPrompt = "Use Door";
    }
    door.deniedPrompt = std::string(GetStringOrEmpty(FindAny(data, {"deniedPrompt", "lockedPrompt"})));
    if (door.deniedPrompt.empty()) {
        door.deniedPrompt = "Locked";
    }
    door.interactionHook = std::string(GetStringOrEmpty(FindAny(data, {"interactionHook", "onInteract"})));
    door.transitionLevel = std::string(GetStringOrEmpty(FindAny(data, {"transitionLevel", "targetLevel", "swapLevel"})));
    door.endingTrigger = std::string(GetStringOrEmpty(FindAny(data, {"endingTrigger", "endTrigger"})));
    door.accessFeedbackTag = std::string(GetStringOrEmpty(FindAny(data, {"accessFeedbackTag", "accessFeedback"})));
    return door;
}

ri::world::CameraConfinementVolume BuildCameraConfinementVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "camera_confinement";
    defaults.type = "camera_confinement_volume";
    defaults.size = {6.0f, 4.0f, 6.0f};
    return ri::world::CreateCameraConfinementVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        defaults);
}

ri::world::LodOverrideVolume BuildLodOverrideVolume(const Value::Object& data) {
    const std::string_view rawLod = GetStringOrEmpty(FindAny(data, {"forcedLod", "lod"}));
    const ri::world::ForcedLod forcedLod = rawLod == "far"
        ? ri::world::ForcedLod::Far
        : ri::world::ForcedLod::Near;

    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "lod_override";
    defaults.type = "lod_override_volume";
    defaults.size = {8.0f, 6.0f, 8.0f};
    return ri::world::CreateLodOverrideVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"targetIds"})),
        forcedLod,
        defaults);
}

ri::world::LodSwitchPrimitive BuildLodSwitchPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "lod_switch";
    defaults.type = "lod_switch_primitive";
    defaults.size = {1.0f, 1.0f, 1.0f};

    std::vector<ri::world::LodSwitchLevel> levels;
    if (const Value* levelsValue = FindAny(data, {"levels"}); levelsValue != nullptr) {
        if (const Value::Array* levelsArray = levelsValue->TryGetArray(); levelsArray != nullptr) {
            levels.reserve(levelsArray->size());
            for (const Value& entry : *levelsArray) {
                const Value::Object* levelObject = entry.TryGetObject();
                if (levelObject == nullptr) {
                    continue;
                }

                ri::world::LodSwitchLevel level{};
                level.name = std::string(GetStringOrEmpty(FindAny(*levelObject, {"name"})));

                if (const Value::Object* representation = FindAny(*levelObject, {"representation"}) == nullptr
                        ? nullptr
                        : FindAny(*levelObject, {"representation"})->TryGetObject();
                    representation != nullptr) {
                    if (const std::optional<ri::world::LodSwitchRepresentationKind> kind =
                            ParseLodSwitchRepresentationKind(FindAny(*representation, {"kind"}));
                        kind.has_value()) {
                        level.representation.kind = *kind;
                    }
                    if (const Value* payload = FindAny(*representation, {"payload"}); payload != nullptr) {
                        if (const Value::Object* payloadObject = payload->TryGetObject(); payloadObject != nullptr) {
                            level.representation.payloadId = std::string(
                                GetStringOrEmpty(FindAny(*payloadObject, {"id", "meshId", "clusterId", "name", "type"})));
                        } else if (const std::string* payloadText = payload->TryGetString(); payloadText != nullptr) {
                            level.representation.payloadId = *payloadText;
                        }
                    }
                }

                if (const std::optional<ri::world::LodSwitchCollisionProfile> collisionProfile =
                        ParseLodSwitchCollisionProfile(FindAny(*levelObject, {"collisionProfile"}));
                    collisionProfile.has_value()) {
                    level.collisionProfile = *collisionProfile;
                }
                level.distanceEnter = static_cast<float>(ReadClampedNumber(*levelObject, {"distanceEnter"}, 0.0, 0.0, 1000000.0));
                level.distanceExit = static_cast<float>(ReadClampedNumber(*levelObject, {"distanceExit"}, 28.0, 0.0, 1000000.0));
                levels.push_back(level);
            }
        }
    }

    ri::world::LodSwitchPolicy policy{};
    if (const Value* policyValue = FindAny(data, {"policy"}); policyValue != nullptr) {
        if (const Value::Object* policyObject = policyValue->TryGetObject(); policyObject != nullptr) {
            if (const std::optional<ri::world::LodSwitchMetric> metric =
                    ParseLodSwitchMetric(FindAny(*policyObject, {"metric"}));
                metric.has_value()) {
                policy.metric = *metric;
            }
            policy.hysteresisEnabled = ReadBoolean(FindAny(*policyObject, {"hysteresisEnabled"}), true);
            if (const std::optional<ri::world::LodSwitchTransitionMode> transitionMode =
                    ParseLodSwitchTransitionMode(FindAny(*policyObject, {"transitionMode"}));
                transitionMode.has_value()) {
                policy.transitionMode = *transitionMode;
            }
            policy.crossfadeSeconds = static_cast<float>(ReadClampedNumber(
                *policyObject, {"crossfadeSeconds"}, 0.0, 0.0, 8.0));
        }
    }

    ri::world::LodSwitchDebugSettings debug{};
    if (const Value* debugValue = FindAny(data, {"debug"}); debugValue != nullptr) {
        if (const Value::Object* debugObject = debugValue->TryGetObject(); debugObject != nullptr) {
            debug.showActiveLevel = ReadBoolean(FindAny(*debugObject, {"showActiveLevel"}), false);
            debug.showRanges = ReadBoolean(FindAny(*debugObject, {"showRanges"}), false);
        }
    }

    Value::Object authoredData = data;
    if (const Value* transformValue = FindAny(data, {"transform"}); transformValue != nullptr) {
        if (const Value::Object* transformObject = transformValue->TryGetObject(); transformObject != nullptr) {
            if (authoredData.find("position") == authoredData.end()) {
                if (const Value* position = FindAny(*transformObject, {"position"}); position != nullptr) {
                    authoredData["position"] = *position;
                }
            }
            if (authoredData.find("rotation") == authoredData.end()) {
                if (const Value* rotation = FindAny(*transformObject, {"rotation"}); rotation != nullptr) {
                    authoredData["rotation"] = *rotation;
                }
            }
            if (authoredData.find("scale") == authoredData.end() && authoredData.find("size") == authoredData.end()) {
                if (const Value* scale = FindAny(*transformObject, {"scale"}); scale != nullptr) {
                    authoredData["scale"] = *scale;
                }
            }
        }
    }

    return ri::world::CreateLodSwitchPrimitive(
        BuildSeedForPhysicsBoxVolume(authoredData, defaults),
        std::move(levels),
        policy,
        debug,
        defaults);
}

ri::world::SurfaceScatterVolume BuildSurfaceScatterVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "surface_scatter";
    defaults.type = "surface_scatter_volume";
    defaults.size = {4.0f, 2.0f, 4.0f};

    std::vector<std::string> targetIds = ParseStringList(FindAny(data, {"targetIds", "targets"}));

    ri::world::SurfaceScatterSourceRepresentation sourceRepresentation{};
    if (const Value* sourceValue = FindAny(data, {"sourceRepresentation", "source"}); sourceValue != nullptr) {
        if (const Value::Object* sourceObject = sourceValue->TryGetObject(); sourceObject != nullptr) {
            if (const std::optional<ri::world::SurfaceScatterRepresentationKind> kind =
                    ParseSurfaceScatterRepresentationKind(FindAny(*sourceObject, {"kind"}));
                kind.has_value()) {
                sourceRepresentation.kind = *kind;
            }
            if (const Value* payload = FindAny(*sourceObject, {"payload"}); payload != nullptr) {
                if (const Value::Object* payloadObject = payload->TryGetObject(); payloadObject != nullptr) {
                    sourceRepresentation.payloadId = std::string(
                        GetStringOrEmpty(FindAny(*payloadObject, {"id", "meshId", "clusterId", "name", "type"})));
                } else if (const std::string* payloadText = payload->TryGetString(); payloadText != nullptr) {
                    sourceRepresentation.payloadId = *payloadText;
                }
            }
            if (sourceRepresentation.payloadId.empty()) {
                sourceRepresentation.payloadId =
                    std::string(GetStringOrEmpty(FindAny(*sourceObject, {"payloadId", "meshId", "clusterId"})));
            }
        }
    }

    ri::world::SurfaceScatterDensityControls density{};
    density.count = static_cast<std::uint32_t>(ReadClampedNumber(data, {"count"}, 64.0, 1.0, 20000.0));
    if (const Value* densityValue = FindAny(data, {"density", "densityControls"}); densityValue != nullptr) {
        if (const Value::Object* densityObject = densityValue->TryGetObject(); densityObject != nullptr) {
            density.count = static_cast<std::uint32_t>(ReadClampedNumber(*densityObject, {"count"}, density.count, 1.0, 20000.0));
            density.densityPerSquareMeter = static_cast<float>(ReadClampedNumber(
                *densityObject, {"densityPerSquareMeter", "density"}, 0.0, 0.0, 500.0));
            density.maxPoints = static_cast<std::uint32_t>(ReadClampedNumber(
                *densityObject, {"maxPoints", "maxCount"}, density.maxPoints, 1.0, 20000.0));
        }
    } else {
        density.densityPerSquareMeter = static_cast<float>(ReadClampedNumber(data, {"densityPerSquareMeter"}, 0.0, 0.0, 500.0));
        density.maxPoints = static_cast<std::uint32_t>(ReadClampedNumber(data, {"maxPoints"}, 2048.0, 1.0, 20000.0));
    }

    ri::world::SurfaceScatterDistributionControls distribution{};
    if (const Value* seedValue = FindAny(data, {"seed"}); seedValue != nullptr) {
        distribution.seed = static_cast<std::uint32_t>(ClampFiniteInteger(*seedValue, 1337, 0, 2147483647));
    }
    if (const Value* distributionValue = FindAny(data, {"distribution"}); distributionValue != nullptr) {
        if (const Value::Object* distributionObject = distributionValue->TryGetObject(); distributionObject != nullptr) {
            if (const Value* seedValue = FindAny(*distributionObject, {"seed"}); seedValue != nullptr) {
                distribution.seed = static_cast<std::uint32_t>(ClampFiniteInteger(*seedValue, 1337, 0, 2147483647));
            }
            distribution.minSlopeDegrees = static_cast<float>(ReadClampedNumber(
                *distributionObject, {"minSlopeDegrees", "slopeMin"}, 0.0, 0.0, 90.0));
            distribution.maxSlopeDegrees = static_cast<float>(ReadClampedNumber(
                *distributionObject, {"maxSlopeDegrees", "slopeMax"}, 85.0, 0.0, 90.0));
            distribution.minHeight = static_cast<float>(ReadClampedNumber(
                *distributionObject, {"minHeight"}, -100000.0, -1000000.0, 1000000.0));
            distribution.maxHeight = static_cast<float>(ReadClampedNumber(
                *distributionObject, {"maxHeight"}, 100000.0, -1000000.0, 1000000.0));
            distribution.minNormalY = static_cast<float>(ReadClampedNumber(
                *distributionObject, {"minNormalY"}, 0.0, -1.0, 1.0));
            distribution.minSeparation = static_cast<float>(ReadClampedNumber(
                *distributionObject, {"minSeparation"}, 0.0, 0.0, 1000.0));
            distribution.rotationJitterRadians =
                ReadVec3(*distributionObject, {"rotationJitterRadians", "rotationJitter"}, {0.0f, 0.0f, 0.0f});
            distribution.scaleJitter = ReadVec3(*distributionObject, {"scaleJitter"}, {0.0f, 0.0f, 0.0f});
            distribution.positionJitter = ReadVec3(*distributionObject, {"positionJitter", "jitter"}, {0.0f, 0.0f, 0.0f});
        }
    }

    ri::world::SurfaceScatterCollisionPolicy collisionPolicy = ri::world::SurfaceScatterCollisionPolicy::None;
    if (const std::optional<ri::world::SurfaceScatterCollisionPolicy> parsedCollisionPolicy =
            ParseSurfaceScatterCollisionPolicy(FindAny(data, {"collisionPolicy"}));
        parsedCollisionPolicy.has_value()) {
        collisionPolicy = *parsedCollisionPolicy;
    }

    ri::world::SurfaceScatterCullingPolicy culling{};
    if (const Value* cullingValue = FindAny(data, {"culling", "cullingPolicy"}); cullingValue != nullptr) {
        if (const Value::Object* cullingObject = cullingValue->TryGetObject(); cullingObject != nullptr) {
            culling.maxActiveDistance = static_cast<float>(ReadClampedNumber(
                *cullingObject, {"maxActiveDistance"}, 80.0, 1.0, 100000.0));
            culling.frustumCulling = ReadBoolean(FindAny(*cullingObject, {"frustumCulling"}), true);
        }
    }

    ri::world::SurfaceScatterAnimationSettings animation{};
    if (const Value* animationValue = FindAny(data, {"animation"}); animationValue != nullptr) {
        if (const Value::Object* animationObject = animationValue->TryGetObject(); animationObject != nullptr) {
            animation.windSwayEnabled = ReadBoolean(FindAny(*animationObject, {"windSwayEnabled"}), false);
            animation.swayAmplitude = static_cast<float>(ReadClampedNumber(
                *animationObject, {"swayAmplitude"}, 0.0, 0.0, 10.0));
            animation.swayFrequency = static_cast<float>(ReadClampedNumber(
                *animationObject, {"swayFrequency"}, 0.0, 0.0, 20.0));
        }
    }

    Value::Object authoredData = data;
    if (const Value* baseTransformValue = FindAny(data, {"baseTransform", "transform"}); baseTransformValue != nullptr) {
        if (const Value::Object* baseTransformObject = baseTransformValue->TryGetObject(); baseTransformObject != nullptr) {
            if (authoredData.find("position") == authoredData.end()) {
                if (const Value* position = FindAny(*baseTransformObject, {"position"}); position != nullptr) {
                    authoredData["position"] = *position;
                }
            }
            if (authoredData.find("rotation") == authoredData.end()) {
                if (const Value* rotation = FindAny(*baseTransformObject, {"rotation"}); rotation != nullptr) {
                    authoredData["rotation"] = *rotation;
                }
            }
            if (authoredData.find("scale") == authoredData.end() && authoredData.find("size") == authoredData.end()) {
                if (const Value* scale = FindAny(*baseTransformObject, {"scale"}); scale != nullptr) {
                    authoredData["scale"] = *scale;
                }
            }
        }
    }

    return ri::world::CreateSurfaceScatterVolume(
        BuildSeedForPhysicsBoxVolume(authoredData, defaults),
        std::move(targetIds),
        sourceRepresentation,
        density,
        distribution,
        collisionPolicy,
        culling,
        animation,
        defaults);
}

ri::world::SplineMeshDeformerPrimitive BuildSplineMeshDeformerPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "spline_mesh_deformer";
    defaults.type = "spline_mesh_deformer";
    defaults.size = {8.0f, 4.0f, 8.0f};

    const std::vector<std::string> targetIds = ParseStringList(FindAny(data, {"targetIds", "targets"}));
    std::vector<ri::math::Vec3> splinePoints = ParseVec3List(FindAny(data, {"spline", "points", "controlPoints", "path"}));
    if (splinePoints.size() < 2U) {
        splinePoints.clear();
    }

    Value::Object authoredData = data;
    if (const Value* transformValue = FindAny(data, {"transform"}); transformValue != nullptr) {
        if (const Value::Object* transformObject = transformValue->TryGetObject(); transformObject != nullptr) {
            if (authoredData.find("position") == authoredData.end()) {
                if (const Value* position = FindAny(*transformObject, {"position"}); position != nullptr) {
                    authoredData["position"] = *position;
                }
            }
            if (authoredData.find("rotation") == authoredData.end()) {
                if (const Value* rotation = FindAny(*transformObject, {"rotation"}); rotation != nullptr) {
                    authoredData["rotation"] = *rotation;
                }
            }
            if (authoredData.find("scale") == authoredData.end() && authoredData.find("size") == authoredData.end()) {
                if (const Value* scale = FindAny(*transformObject, {"scale"}); scale != nullptr) {
                    authoredData["scale"] = *scale;
                }
            }
        }
    }

    return ri::world::CreateSplineMeshDeformerPrimitive(
        BuildSeedForPhysicsBoxVolume(authoredData, defaults),
        targetIds,
        splinePoints,
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"count", "sampleCount"}, 16.0, 2.0, 2048.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"sectionCount"}, 1.0, 1.0, 256.0)),
        static_cast<float>(ReadClampedNumber(data, {"segmentLength"}, 2.0, 0.05, 128.0)),
        static_cast<float>(ReadClampedNumber(data, {"tangentSmoothing", "tension"}, 0.5, 0.0, 1.0)),
        ReadBoolean(FindAny(data, {"keepSource"}), false),
        ReadBoolean(FindAny(data, {"collisionEnabled", "isCollider"}), false),
        ReadBoolean(FindAny(data, {"navInfluence"}), false),
        ReadBoolean(FindAny(data, {"dynamicEnabled"}), false),
        [](const Value::Object& source) -> std::uint32_t {
            if (const Value* seedValue = FindAny(source, {"seed"}); seedValue != nullptr) {
                return static_cast<std::uint32_t>(ClampFiniteInteger(*seedValue, 1337, 0, 2147483647));
            }
            return 1337U;
        }(data),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"maxSamples", "sampleCap"}, 256.0, 2.0, 4096.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 120.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::SplineDecalRibbonPrimitive BuildSplineDecalRibbonPrimitive(const Value::Object& data) {
    const std::string_view rawType = GetStringOrEmpty(FindAny(data, {"type"}));
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "spline_decal_ribbon";
    defaults.type = (rawType == "spline_ribbon") ? "spline_ribbon" : "spline_decal_ribbon";
    defaults.size = {8.0f, 2.0f, 8.0f};

    std::vector<ri::math::Vec3> splinePoints = ParseVec3List(FindAny(data, {"spline", "points", "controlPoints", "path"}));
    if (splinePoints.size() < 2U) {
        splinePoints.clear();
    }

    Value::Object authoredData = data;
    if (const Value* transformValue = FindAny(data, {"transform"}); transformValue != nullptr) {
        if (const Value::Object* transformObject = transformValue->TryGetObject(); transformObject != nullptr) {
            if (authoredData.find("position") == authoredData.end()) {
                if (const Value* position = FindAny(*transformObject, {"position"}); position != nullptr) {
                    authoredData["position"] = *position;
                }
            }
            if (authoredData.find("rotation") == authoredData.end()) {
                if (const Value* rotation = FindAny(*transformObject, {"rotation"}); rotation != nullptr) {
                    authoredData["rotation"] = *rotation;
                }
            }
            if (authoredData.find("scale") == authoredData.end() && authoredData.find("size") == authoredData.end()) {
                if (const Value* scale = FindAny(*transformObject, {"scale"}); scale != nullptr) {
                    authoredData["scale"] = *scale;
                }
            }
        }
    }

    return ri::world::CreateSplineDecalRibbonPrimitive(
        BuildSeedForPhysicsBoxVolume(authoredData, defaults),
        splinePoints,
        static_cast<float>(ReadClampedNumber(data, {"width"}, 1.0, 0.01, 128.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"tessellationFactor", "tessellation", "segments"}, 32.0, 2.0, 4096.0)),
        static_cast<float>(ReadClampedNumber(data, {"offsetY"}, 0.03, -10.0, 10.0)),
        static_cast<float>(ReadClampedNumber(data, {"uvScaleU"}, 1.0, 0.01, 1000.0)),
        static_cast<float>(ReadClampedNumber(data, {"uvScaleV"}, 1.0, 0.01, 1000.0)),
        static_cast<float>(ReadClampedNumber(data, {"tangentSmoothing", "tension"}, 0.5, 0.0, 1.0)),
        ReadBoolean(FindAny(data, {"transparentBlend"}), true),
        ReadBoolean(FindAny(data, {"depthWrite"}), false),
        ReadBoolean(FindAny(data, {"collisionEnabled", "isCollider"}), false),
        ReadBoolean(FindAny(data, {"navInfluence"}), false),
        ReadBoolean(FindAny(data, {"dynamicEnabled"}), false),
        [](const Value::Object& source) -> std::uint32_t {
            if (const Value* seedValue = FindAny(source, {"seed"}); seedValue != nullptr) {
                return static_cast<std::uint32_t>(ClampFiniteInteger(*seedValue, 1337, 0, 2147483647));
            }
            return 1337U;
        }(data),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"maxSamples", "sampleCap"}, 256.0, 2.0, 4096.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 120.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::TopologicalUvRemapperVolume BuildTopologicalUvRemapperVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "topological_uv_remapper";
    defaults.type = "topological_uv_remapper";
    defaults.size = {12.0f, 4.0f, 12.0f};

    std::vector<std::string> targetIds = ParseStringList(FindAny(data, {"targetIds", "targets"}));
    std::string textureX;
    std::string textureY;
    std::string textureZ;
    std::string sharedTextureId;
    ParseProceduralUvTextureAssignments(data, textureX, textureY, textureZ, sharedTextureId);
    const ri::world::ProceduralUvProjectionDebugControls debug = ParseProceduralUvDebugControls(data);

    Value::Object authoredData = data;
    if (const Value* transformValue = FindAny(data, {"transform"}); transformValue != nullptr) {
        if (const Value::Object* transformObject = transformValue->TryGetObject(); transformObject != nullptr) {
            if (authoredData.find("position") == authoredData.end()) {
                if (const Value* position = FindAny(*transformObject, {"position"}); position != nullptr) {
                    authoredData["position"] = *position;
                }
            }
            if (authoredData.find("rotation") == authoredData.end()) {
                if (const Value* rotation = FindAny(*transformObject, {"rotation"}); rotation != nullptr) {
                    authoredData["rotation"] = *rotation;
                }
            }
            if (authoredData.find("scale") == authoredData.end() && authoredData.find("size") == authoredData.end()) {
                if (const Value* scale = FindAny(*transformObject, {"scale"}); scale != nullptr) {
                    authoredData["scale"] = *scale;
                }
            }
        }
    }

    return ri::world::CreateTopologicalUvRemapperVolume(
        BuildSeedForPhysicsBoxVolume(authoredData, defaults),
        std::move(targetIds),
        std::string(GetStringOrEmpty(FindAny(data, {"remapMode", "mode"}))),
        std::move(textureX),
        std::move(textureY),
        std::move(textureZ),
        std::move(sharedTextureId),
        static_cast<float>(ReadClampedNumber(data, {"projectionScale", "scale", "uvScale"}, 1.0, 0.000001, 4096.0)),
        static_cast<float>(ReadClampedNumber(data, {"blendSharpness", "sharpness", "blend"}, 4.0, 0.25, 64.0)),
        ReadVec3(data, {"axisWeights", "weights", "axisWeight"}, {1.0f, 1.0f, 1.0f}),
        static_cast<std::uint32_t>(
            ReadClampedNumber(data, {"maxMaterialPatches", "patchBudget", "maxPatches"}, 256.0, 1.0, 4096.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 512.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        debug,
        defaults);
}

ri::world::TriPlanarNode BuildTriPlanarNode(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "tri_planar_node";
    defaults.type = "tri_planar_node";
    defaults.size = {10.0f, 4.0f, 10.0f};

    std::vector<std::string> targetIds = ParseStringList(FindAny(data, {"targetIds", "targets"}));
    std::string textureX;
    std::string textureY;
    std::string textureZ;
    std::string sharedTextureId;
    ParseProceduralUvTextureAssignments(data, textureX, textureY, textureZ, sharedTextureId);
    const ri::world::ProceduralUvProjectionDebugControls debug = ParseProceduralUvDebugControls(data);

    Value::Object authoredData = data;
    if (const Value* transformValue = FindAny(data, {"transform"}); transformValue != nullptr) {
        if (const Value::Object* transformObject = transformValue->TryGetObject(); transformObject != nullptr) {
            if (authoredData.find("position") == authoredData.end()) {
                if (const Value* position = FindAny(*transformObject, {"position"}); position != nullptr) {
                    authoredData["position"] = *position;
                }
            }
            if (authoredData.find("rotation") == authoredData.end()) {
                if (const Value* rotation = FindAny(*transformObject, {"rotation"}); rotation != nullptr) {
                    authoredData["rotation"] = *rotation;
                }
            }
            if (authoredData.find("scale") == authoredData.end() && authoredData.find("size") == authoredData.end()) {
                if (const Value* scale = FindAny(*transformObject, {"scale"}); scale != nullptr) {
                    authoredData["scale"] = *scale;
                }
            }
        }
    }

    return ri::world::CreateTriPlanarNode(
        BuildSeedForPhysicsBoxVolume(authoredData, defaults),
        std::move(targetIds),
        std::move(textureX),
        std::move(textureY),
        std::move(textureZ),
        std::move(sharedTextureId),
        static_cast<float>(ReadClampedNumber(data, {"projectionScale", "scale", "uvScale"}, 1.0, 0.000001, 4096.0)),
        static_cast<float>(ReadClampedNumber(data, {"blendSharpness", "sharpness", "blend"}, 4.0, 0.25, 64.0)),
        ReadVec3(data, {"axisWeights", "weights", "axisWeight"}, {1.0f, 1.0f, 1.0f}),
        static_cast<std::uint32_t>(
            ReadClampedNumber(data, {"maxMaterialPatches", "patchBudget", "maxPatches"}, 256.0, 1.0, 4096.0)),
        ReadBoolean(FindAny(data, {"objectSpaceAxes", "objectSpace"}), false),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 512.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        debug,
        defaults);
}

ri::world::InstanceCloudPrimitive BuildInstanceCloudPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "instance_cloud";
    defaults.type = "instance_cloud_primitive";
    defaults.size = {2.0f, 2.0f, 2.0f};

    ri::world::InstanceCloudSourceRepresentation sourceRepresentation{};
    if (const Value* sourceValue = FindAny(data, {"sourceRepresentation", "source"}); sourceValue != nullptr) {
        if (const Value::Object* sourceObject = sourceValue->TryGetObject(); sourceObject != nullptr) {
            if (const std::optional<ri::world::InstanceCloudRepresentationKind> kind =
                    ParseInstanceCloudRepresentationKind(FindAny(*sourceObject, {"kind"}));
                kind.has_value()) {
                sourceRepresentation.kind = *kind;
            }

            if (const Value* payload = FindAny(*sourceObject, {"payload"}); payload != nullptr) {
                if (const Value::Object* payloadObject = payload->TryGetObject(); payloadObject != nullptr) {
                    sourceRepresentation.payloadId = std::string(
                        GetStringOrEmpty(FindAny(*payloadObject, {"id", "meshId", "clusterId", "name", "type"})));
                } else if (const std::string* payloadText = payload->TryGetString(); payloadText != nullptr) {
                    sourceRepresentation.payloadId = *payloadText;
                }
            }

            if (sourceRepresentation.payloadId.empty()) {
                sourceRepresentation.payloadId = std::string(
                    GetStringOrEmpty(FindAny(*sourceObject, {"payloadId", "meshId", "clusterId"})));
            }
        }
    }

    const std::uint32_t count = static_cast<std::uint32_t>(ReadClampedNumber(data, {"count"}, 1.0, 1.0, 20000.0));
    const ri::math::Vec3 offsetStep = ReadVec3(data, {"offsetStep", "step"}, {0.0f, 0.0f, 0.0f});
    const ri::math::Vec3 distributionExtents =
        ReadVec3(data, {"distributionExtents", "distribution", "extents"}, {0.0f, 0.0f, 0.0f});

    std::uint32_t seed = 1337U;
    if (const Value* seedValue = FindAny(data, {"seed"}); seedValue != nullptr) {
        seed = static_cast<std::uint32_t>(ClampFiniteInteger(*seedValue, 1337, 0, 2147483647));
    }

    ri::world::InstanceCloudVariationRanges variation{};
    if (const Value* variationValue = FindAny(data, {"variation"}); variationValue != nullptr) {
        if (const Value::Object* variationObject = variationValue->TryGetObject(); variationObject != nullptr) {
            variation.rotationJitterRadians =
                ReadVec3(*variationObject, {"rotationJitterRadians", "rotationJitter"}, {0.0f, 0.0f, 0.0f});
            variation.scaleJitter = ReadVec3(*variationObject, {"scaleJitter"}, {0.0f, 0.0f, 0.0f});
            variation.positionJitter =
                ReadVec3(*variationObject, {"positionJitter", "jitter"}, {0.0f, 0.0f, 0.0f});
        }
    }

    ri::world::InstanceCloudCollisionPolicy collisionPolicy = ri::world::InstanceCloudCollisionPolicy::None;
    if (const std::optional<ri::world::InstanceCloudCollisionPolicy> parsedCollisionPolicy =
            ParseInstanceCloudCollisionPolicy(FindAny(data, {"collisionPolicy"}));
        parsedCollisionPolicy.has_value()) {
        collisionPolicy = *parsedCollisionPolicy;
    }

    ri::world::InstanceCloudCullingPolicy culling{};
    if (const Value* cullingValue = FindAny(data, {"culling", "cullingPolicy"}); cullingValue != nullptr) {
        if (const Value::Object* cullingObject = cullingValue->TryGetObject(); cullingObject != nullptr) {
            culling.maxActiveDistance = static_cast<float>(ReadClampedNumber(
                *cullingObject,
                {"maxActiveDistance"},
                80.0,
                1.0,
                100000.0));
            culling.frustumCulling = ReadBoolean(FindAny(*cullingObject, {"frustumCulling"}), true);
        }
    }

    Value::Object authoredData = data;
    if (const Value* baseTransformValue = FindAny(data, {"baseTransform"}); baseTransformValue != nullptr) {
        if (const Value::Object* baseTransformObject = baseTransformValue->TryGetObject(); baseTransformObject != nullptr) {
            if (authoredData.find("position") == authoredData.end()) {
                if (const Value* position = FindAny(*baseTransformObject, {"position"}); position != nullptr) {
                    authoredData["position"] = *position;
                }
            }
            if (authoredData.find("rotation") == authoredData.end()) {
                if (const Value* rotation = FindAny(*baseTransformObject, {"rotation"}); rotation != nullptr) {
                    authoredData["rotation"] = *rotation;
                }
            }
            if (authoredData.find("scale") == authoredData.end() && authoredData.find("size") == authoredData.end()) {
                if (const Value* scale = FindAny(*baseTransformObject, {"scale"}); scale != nullptr) {
                    authoredData["scale"] = *scale;
                }
            }
        }
    }

    return ri::world::CreateInstanceCloudPrimitive(
        BuildSeedForPhysicsBoxVolume(authoredData, defaults),
        sourceRepresentation,
        count,
        offsetStep,
        distributionExtents,
        seed,
        variation,
        collisionPolicy,
        culling,
        defaults);
}

ri::world::VoronoiFracturePrimitive BuildVoronoiFracturePrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "voronoi_fracture";
    defaults.type = "voronoi_fracture_primitive";
    defaults.size = {6.0f, 4.0f, 6.0f};
    return ri::world::CreateVoronoiFracturePrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"targetIds", "targets"})),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"cellCount", "cells"}, 16.0, 2.0, 1024.0)),
        static_cast<float>(ReadClampedNumber(data, {"noiseJitter", "jitter"}, 0.1, 0.0, 1.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"seed"}, 1337.0, 0.0, 2147483647.0)),
        ReadBoolean(FindAny(data, {"capOpenFaces", "capFaces"}), true),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::MetaballPrimitive BuildMetaballPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "metaball";
    defaults.type = "metaball_primitive";
    defaults.shape = ri::world::VolumeShape::Sphere;
    defaults.size = {4.0f, 4.0f, 4.0f};
    return ri::world::CreateMetaballPrimitive(
        BuildSeedWithSizeAliases(data, defaults),
        ParseVec3List(FindAny(data, {"controlPoints", "points", "metaballs"})),
        static_cast<float>(ReadClampedNumber(data, {"isoLevel", "threshold"}, 0.5, 0.01, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"smoothing", "blend"}, 0.35, 0.0, 2.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"resolution"}, 24.0, 4.0, 256.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::LatticeVolume BuildLatticeVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "lattice";
    defaults.type = "lattice_volume";
    defaults.size = {8.0f, 4.0f, 8.0f};
    return ri::world::CreateLatticeVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"targetIds", "targets"})),
        ReadVec3(data, {"cellSize", "cell", "grid"}, {0.5f, 0.5f, 0.5f}),
        static_cast<float>(ReadClampedNumber(data, {"beamRadius", "thickness"}, 0.08, 0.001, 10.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"maxCells", "cellBudget"}, 2048.0, 8.0, 65536.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 120.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::ManifoldSweepPrimitive BuildManifoldSweepPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "manifold_sweep";
    defaults.type = "manifold_sweep";
    defaults.size = {8.0f, 4.0f, 8.0f};
    return ri::world::CreateManifoldSweepPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"targetIds", "targets"})),
        ParseVec3List(FindAny(data, {"spline", "points", "controlPoints", "path"})),
        static_cast<float>(ReadClampedNumber(data, {"profileRadius", "radius"}, 0.25, 0.001, 16.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"sampleCount", "count"}, 32.0, 2.0, 2048.0)),
        ReadBoolean(FindAny(data, {"capEnds"}), true),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 128.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::TrimSheetSweepPrimitive BuildTrimSheetSweepPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "trim_sheet_sweep";
    defaults.type = "trim_sheet_sweep";
    defaults.size = {8.0f, 2.0f, 8.0f};
    return ri::world::CreateTrimSheetSweepPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"targetIds", "targets"})),
        ParseVec3List(FindAny(data, {"spline", "points", "controlPoints", "path"})),
        std::string(GetStringOrEmpty(FindAny(data, {"trimSheetId", "trimSheet", "sheetId"}))),
        static_cast<float>(ReadClampedNumber(data, {"uvTileU", "tileU"}, 1.0, 0.001, 1024.0)),
        static_cast<float>(ReadClampedNumber(data, {"uvTileV", "tileV"}, 1.0, 0.001, 1024.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"tessellation", "segments"}, 24.0, 2.0, 4096.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 128.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::LSystemBranchPrimitive BuildLSystemBranchPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "l_system_branch";
    defaults.type = "l_system_branch_primitive";
    defaults.size = {8.0f, 6.0f, 8.0f};
    return ri::world::CreateLSystemBranchPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"targetIds", "targets"})),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"iterations", "depth"}, 4.0, 1.0, 10.0)),
        static_cast<float>(ReadClampedNumber(data, {"segmentLength", "length"}, 0.5, 0.01, 64.0)),
        static_cast<float>(ReadClampedNumber(data, {"branchAngleDegrees", "angle"}, 22.5, 0.0, 180.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"seed"}, 1337.0, 0.0, 2147483647.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 120.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::GeodesicSpherePrimitive BuildGeodesicSpherePrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "geodesic_sphere";
    defaults.type = "geodesic_sphere";
    defaults.shape = ri::world::VolumeShape::Sphere;
    defaults.size = {4.0f, 4.0f, 4.0f};
    return ri::world::CreateGeodesicSpherePrimitive(
        BuildSeedWithSizeAliases(data, defaults),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"subdivisionLevel", "detail"}, 2.0, 0.0, 8.0)),
        static_cast<float>(ReadClampedNumber(data, {"radiusScale", "radius"}, 1.0, 0.001, 1000.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 120.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::ExtrudeAlongNormalPrimitive BuildExtrudeAlongNormalPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "extrude_along_normal";
    defaults.type = "extrude_along_normal_primitive";
    defaults.size = {6.0f, 4.0f, 6.0f};
    return ri::world::CreateExtrudeAlongNormalPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"targetIds", "targets"})),
        static_cast<float>(ReadClampedNumber(data, {"distance", "amount"}, 0.2, -100.0, 100.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"shellCount", "layers"}, 1.0, 1.0, 256.0)),
        ReadBoolean(FindAny(data, {"capOpenEdges", "capEdges"}), true),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 120.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::SuperellipsoidPrimitive BuildSuperellipsoidPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "superellipsoid";
    defaults.type = "superellipsoid";
    defaults.shape = ri::world::VolumeShape::Sphere;
    defaults.size = {4.0f, 4.0f, 4.0f};
    return ri::world::CreateSuperellipsoidPrimitive(
        BuildSeedWithSizeAliases(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"exponentX", "powerX"}, 2.0, 0.1, 16.0)),
        static_cast<float>(ReadClampedNumber(data, {"exponentY", "powerY"}, 2.0, 0.1, 16.0)),
        static_cast<float>(ReadClampedNumber(data, {"exponentZ", "powerZ"}, 2.0, 0.1, 16.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"radialSegments", "segments"}, 24.0, 3.0, 256.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"rings"}, 16.0, 2.0, 256.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 120.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::PrimitiveDemoLattice BuildPrimitiveDemoLattice(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "primitive_demo_lattice";
    defaults.type = "primitive_demo_lattice";
    defaults.size = {8.0f, 4.0f, 8.0f};
    return ri::world::CreatePrimitiveDemoLattice(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"targetIds", "targets"})),
        ReadVec3(data, {"cellSize", "cell", "grid"}, {0.6f, 0.6f, 0.6f}),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"maxCells", "cellBudget"}, 1024.0, 8.0, 65536.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::PrimitiveDemoVoronoi BuildPrimitiveDemoVoronoi(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "primitive_demo_voronoi";
    defaults.type = "primitive_demo_voronoi";
    defaults.size = {8.0f, 4.0f, 8.0f};
    return ri::world::CreatePrimitiveDemoVoronoi(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"targetIds", "targets"})),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"cellCount", "cells"}, 12.0, 2.0, 1024.0)),
        static_cast<float>(ReadClampedNumber(data, {"jitter", "noiseJitter"}, 0.1, 0.0, 1.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"seed"}, 1337.0, 0.0, 2147483647.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::ThickPolygonPrimitive BuildThickPolygonPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "thick_polygon";
    defaults.type = "thick_polygon_primitive";
    defaults.size = {6.0f, 2.0f, 6.0f};
    return ri::world::CreateThickPolygonPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseVec3List(FindAny(data, {"points", "polygon", "vertices"})),
        static_cast<float>(ReadClampedNumber(data, {"thickness", "depth"}, 0.2, 0.001, 100.0)),
        ReadBoolean(FindAny(data, {"capTop"}), true),
        ReadBoolean(FindAny(data, {"capBottom"}), true),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::StructuralProfilePrimitive BuildStructuralProfilePrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "structural_profile";
    defaults.type = "structural_profile";
    defaults.size = {6.0f, 4.0f, 6.0f};
    return ri::world::CreateStructuralProfilePrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        std::string(GetStringOrEmpty(FindAny(data, {"profileId", "profile", "shapeProfile"}))),
        static_cast<float>(ReadClampedNumber(data, {"profileScale", "scale"}, 1.0, 0.001, 1000.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"segmentCount", "segments"}, 16.0, 2.0, 4096.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::HalfPipePrimitive BuildHalfPipePrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "half_pipe";
    defaults.type = "half_pipe";
    defaults.size = {8.0f, 4.0f, 8.0f};
    return ri::world::CreateHalfPipePrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"radius"}, 2.0, 0.01, 1000.0)),
        static_cast<float>(ReadClampedNumber(data, {"length"}, 6.0, 0.01, 10000.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"radialSegments", "segments"}, 16.0, 3.0, 512.0)),
        static_cast<float>(ReadClampedNumber(data, {"wallThickness", "thickness"}, 0.2, 0.001, 100.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::QuarterPipePrimitive BuildQuarterPipePrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "quarter_pipe";
    defaults.type = "quarter_pipe";
    defaults.size = {8.0f, 4.0f, 8.0f};
    return ri::world::CreateQuarterPipePrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"radius"}, 2.0, 0.01, 1000.0)),
        static_cast<float>(ReadClampedNumber(data, {"length"}, 6.0, 0.01, 10000.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"radialSegments", "segments"}, 12.0, 3.0, 512.0)),
        static_cast<float>(ReadClampedNumber(data, {"wallThickness", "thickness"}, 0.2, 0.001, 100.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::PipeElbowPrimitive BuildPipeElbowPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "pipe_elbow";
    defaults.type = "pipe_elbow";
    defaults.size = {6.0f, 4.0f, 6.0f};
    return ri::world::CreatePipeElbowPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"radius"}, 1.0, 0.01, 1000.0)),
        static_cast<float>(ReadClampedNumber(data, {"bendDegrees", "angle"}, 90.0, 1.0, 359.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"radialSegments", "segments"}, 16.0, 3.0, 512.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"bendSegments"}, 12.0, 1.0, 512.0)),
        static_cast<float>(ReadClampedNumber(data, {"wallThickness", "thickness"}, 0.15, 0.001, 100.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::TorusSlicePrimitive BuildTorusSlicePrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "torus_slice";
    defaults.type = "torus_slice";
    defaults.size = {6.0f, 4.0f, 6.0f};
    return ri::world::CreateTorusSlicePrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"majorRadius", "radius"}, 2.0, 0.01, 1000.0)),
        static_cast<float>(ReadClampedNumber(data, {"minorRadius", "tubeRadius"}, 0.5, 0.001, 500.0)),
        static_cast<float>(ReadClampedNumber(data, {"sweepDegrees", "angle"}, 180.0, 1.0, 360.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"radialSegments"}, 24.0, 3.0, 512.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"tubularSegments", "segments"}, 16.0, 3.0, 512.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::SplineSweepPrimitive BuildSplineSweepPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "spline_sweep";
    defaults.type = "spline_sweep";
    defaults.size = {8.0f, 4.0f, 8.0f};
    return ri::world::CreateSplineSweepPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseStringList(FindAny(data, {"targetIds", "targets"})),
        ParseVec3List(FindAny(data, {"spline", "points", "controlPoints", "path"})),
        static_cast<float>(ReadClampedNumber(data, {"profileRadius", "radius"}, 0.25, 0.001, 16.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"sampleCount", "count"}, 32.0, 2.0, 4096.0)),
        ReadBoolean(FindAny(data, {"capEnds"}), true),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 120.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::RevolvePrimitive BuildRevolvePrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "revolve";
    defaults.type = "revolve";
    defaults.size = {6.0f, 4.0f, 6.0f};
    return ri::world::CreateRevolvePrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseVec3List(FindAny(data, {"profilePoints", "points", "profile"})),
        static_cast<float>(ReadClampedNumber(data, {"sweepDegrees", "angle"}, 360.0, 1.0, 360.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"segmentCount", "segments"}, 24.0, 3.0, 4096.0)),
        ReadBoolean(FindAny(data, {"capEnds"}), false),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::DomeVaultPrimitive BuildDomeVaultPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "dome_vault";
    defaults.type = "dome_vault";
    defaults.size = {8.0f, 4.0f, 8.0f};
    return ri::world::CreateDomeVaultPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"radius"}, 4.0, 0.01, 1000.0)),
        static_cast<float>(ReadClampedNumber(data, {"thickness"}, 0.25, 0.001, 100.0)),
        static_cast<float>(ReadClampedNumber(data, {"heightRatio", "heightScale"}, 0.5, 0.01, 1.0)),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"radialSegments", "segments"}, 24.0, 3.0, 1024.0)),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 96.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::LoftPrimitive BuildLoftPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "loft";
    defaults.type = "loft_primitive";
    defaults.size = {8.0f, 4.0f, 8.0f};
    return ri::world::CreateLoftPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ParseVec3List(FindAny(data, {"pathPoints", "path", "spline"})),
        ParseVec3List(FindAny(data, {"profilePoints", "profile"})),
        static_cast<std::uint32_t>(ReadClampedNumber(data, {"segmentCount", "segments"}, 24.0, 2.0, 4096.0)),
        ReadBoolean(FindAny(data, {"capEnds"}), true),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 120.0, 1.0, 100000.0)),
        ReadBoolean(FindAny(data, {"frustumCulling"}), true),
        defaults);
}

ri::world::NavmeshModifierVolume BuildNavmeshModifierVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "navmesh_modifier";
    defaults.type = "navmesh_modifier_volume";
    defaults.size = {6.0f, 4.0f, 6.0f};
    return ri::world::CreateNavmeshModifierVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"traversalCost", "cost", "weight"}, 1.5, 0.01, 100.0)),
        std::string(GetStringOrEmpty(FindAny(data, {"tag", "areaType"}))).empty()
            ? "modified"
            : std::string(GetStringOrEmpty(FindAny(data, {"tag", "areaType"}))),
        defaults);
}

ri::world::VisibilityPrimitive BuildVisibilityPrimitive(const Value::Object& data) {
    const std::string_view rawType = GetStringOrEmpty(FindAny(data, {"type"}));
    const bool antiPortal = rawType == "anti_portal";

    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = antiPortal ? "anti_portal" : "portal";
    defaults.type = antiPortal ? "anti_portal" : "portal";
    defaults.size = {1.0f, 1.0f, 1.0f};

    ri::world::RuntimeVolumeSeed seed = BuildSeedWithSizeAliases(data, defaults);
    ClampMinimumExtents(seed);
    return ri::world::CreateVisibilityPrimitive(
        seed,
        antiPortal ? ri::world::VisibilityPrimitiveKind::AntiPortal : ri::world::VisibilityPrimitiveKind::Portal,
        defaults);
}

ri::world::ReflectionProbeVolume BuildReflectionProbeVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "reflection_probe";
    defaults.type = "reflection_probe_volume";
    defaults.size = {6.0f, 6.0f, 6.0f};
    const Value* captureResolutionValue = FindAny(data, {"captureResolution", "resolution", "probeResolution"});
    const int captureResolution = captureResolutionValue == nullptr
        ? 256
        : ClampFiniteInteger(*captureResolutionValue, 256, 64, 2048);
    return ri::world::CreateReflectionProbeVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"intensity", "probeIntensity"}, 1.0, 0.0, 8.0)),
        static_cast<float>(ReadClampedNumber(data, {"blendDistance", "blendRadius"}, 1.5, 0.0, 64.0)),
        static_cast<std::uint32_t>(captureResolution),
        ReadBoolean(FindAny(data, {"boxProjection", "parallaxCorrection"}), true),
        ReadBoolean(FindAny(data, {"dynamicCapture", "dynamic"}), false),
        defaults);
}

ri::world::LightImportanceVolume BuildLightImportanceVolume(const Value::Object& data) {
    const std::string_view rawType = GetStringOrEmpty(FindAny(data, {"type"}));
    const bool probeGridBounds = (rawType == "probe_grid_bounds" || rawType == "probe_grid");

    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "light_importance";
    defaults.type = probeGridBounds ? "probe_grid_bounds" : "light_importance_volume";
    defaults.size = {8.0f, 6.0f, 8.0f};
    return ri::world::CreateLightImportanceVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        probeGridBounds,
        defaults);
}

ri::world::LightPortalVolume BuildLightPortalVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "light_portal";
    defaults.type = "light_portal";
    defaults.size = {5.0f, 5.0f, 1.0f};
    return ri::world::CreateLightPortalVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"transmission", "portalTransmission"}, 1.0, 0.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"softness", "edgeSoftness"}, 0.1, 0.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"priority"}, 0.0, -100.0, 100.0)),
        ReadBoolean(FindAny(data, {"twoSided", "doubleSided"}), false),
        defaults);
}

ri::world::VoxelGiBoundsVolume BuildVoxelGiBoundsVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "voxel_gi_bounds";
    defaults.type = "voxel_gi_bounds";
    defaults.size = {16.0f, 12.0f, 16.0f};
    const Value* cascadeCountValue = FindAny(data, {"cascadeCount", "cascades"});
    const int cascadeCount = cascadeCountValue == nullptr ? 1 : ClampFiniteInteger(*cascadeCountValue, 1, 1, 8);
    return ri::world::CreateVoxelGiBoundsVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"voxelSize", "voxel_size"}, 1.0, 0.05, 16.0)),
        static_cast<std::uint32_t>(cascadeCount),
        ReadBoolean(FindAny(data, {"updateDynamics", "dynamic"}), true),
        defaults);
}

ri::world::LightmapDensityVolume BuildLightmapDensityVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "lightmap_density_volume";
    defaults.type = "lightmap_density_volume";
    defaults.size = {8.0f, 6.0f, 8.0f};
    return ri::world::CreateLightmapDensityVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"texelsPerMeter", "density"}, 256.0, 4.0, 4096.0)),
        static_cast<float>(ReadClampedNumber(data, {"minimumTexelsPerMeter", "minTexelsPerMeter", "minDensity"}, 64.0, 1.0, 4096.0)),
        static_cast<float>(ReadClampedNumber(data, {"maximumTexelsPerMeter", "maxTexelsPerMeter", "maxDensity"}, 1024.0, 1.0, 4096.0)),
        ReadBoolean(FindAny(data, {"clampBySurfaceArea", "surfaceAreaClamp"}), true),
        defaults);
}

ri::world::ShadowExclusionVolume BuildShadowExclusionVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "shadow_exclusion";
    defaults.type = "shadow_exclusion_volume";
    defaults.size = {8.0f, 6.0f, 8.0f};
    return ri::world::CreateShadowExclusionVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ReadBoolean(FindAny(data, {"excludeStaticShadows", "excludeStatic"}), true),
        ReadBoolean(FindAny(data, {"excludeDynamicShadows", "excludeDynamic"}), true),
        ReadBoolean(FindAny(data, {"affectVolumetricShadows", "excludeVolumetric"}), false),
        static_cast<float>(ReadClampedNumber(data, {"fadeDistance", "fade"}, 0.5, 0.0, 100.0)),
        defaults);
}

ri::world::CullingDistanceVolume BuildCullingDistanceVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "culling_distance";
    defaults.type = "culling_distance_volume";
    defaults.size = {12.0f, 8.0f, 12.0f};
    return ri::world::CreateCullingDistanceVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"nearDistance", "minDistance"}, 0.0, 0.0, 100000.0)),
        static_cast<float>(ReadClampedNumber(data, {"farDistance", "maxDistance"}, 128.0, 0.0, 100000.0)),
        ReadBoolean(FindAny(data, {"applyToStaticObjects", "static"}), true),
        ReadBoolean(FindAny(data, {"applyToDynamicObjects", "dynamic"}), true),
        ReadBoolean(FindAny(data, {"allowHlod", "allowHLOD"}), true),
        defaults);
}

ri::world::ReferenceImagePlane BuildReferenceImagePlane(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "reference_image_plane";
    defaults.type = "reference_image_plane";
    defaults.size = {8.0f, 5.0f, 1.0f};
    return ri::world::CreateReferenceImagePlane(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        std::string(GetStringOrEmpty(FindAny(data, {"textureId", "texture", "imageId", "textureUrl"}))),
        std::string(GetStringOrEmpty(FindAny(data, {"imageUrl"}))),
        ReadColor(data, {"color", "tintColor"}, {1.0f, 1.0f, 1.0f}),
        static_cast<float>(ReadClampedNumber(data, {"opacity"}, 0.88, 0.05, 1.0)),
        FindAny(data, {"renderOrder"}) == nullptr
            ? 60
            : ClampFiniteInteger(*FindAny(data, {"renderOrder"}), 60, 1, 200),
        ReadBoolean(FindAny(data, {"alwaysFaceCamera"}), false),
        defaults);
}

ri::world::Text3dPrimitive BuildText3dPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "text_3d";
    defaults.type = "text_3d_primitive";
    defaults.size = {2.0f, 2.0f, 0.4f};
    return ri::world::CreateText3dPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        std::string(GetStringOrEmpty(FindAny(data, {"text", "value", "label"}))),
        std::string(GetStringOrEmpty(FindAny(data, {"fontFamily", "font", "fontName"}))),
        std::string(GetStringOrEmpty(FindAny(data, {"materialId", "material", "textMaterial"}))),
        std::string(GetStringOrEmpty(FindAny(data, {"textColor", "color"}))),
        std::string(GetStringOrEmpty(FindAny(data, {"outlineColor", "strokeColor"}))),
        static_cast<float>(ReadClampedNumber(data, {"textScale"}, 1.0, 0.05, 48.0)),
        static_cast<float>(ReadClampedNumber(data, {"depth", "extrudeDepth"}, 0.08, 0.001, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"extrusionBevel", "bevel"}, 0.02, 0.0, 1.0)),
        static_cast<float>(ReadClampedNumber(data, {"letterSpacing", "tracking"}, 0.0, -2.0, 8.0)),
        ReadBoolean(FindAny(data, {"alwaysFaceCamera", "billboard"}), false),
        ReadBoolean(FindAny(data, {"doubleSided"}), true),
        defaults);
}

ri::world::AnnotationCommentPrimitive BuildAnnotationCommentPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "annotation_comment";
    defaults.type = "annotation_comment_primitive";
    defaults.size = {2.0f, 2.0f, 2.0f};
    return ri::world::CreateAnnotationCommentPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        std::string(GetStringOrEmpty(FindAny(data, {"text", "note", "comment"}))),
        std::string(GetStringOrEmpty(FindAny(data, {"accentColor"}))),
        std::string(GetStringOrEmpty(FindAny(data, {"backgroundColor"}))),
        static_cast<float>(ReadClampedNumber(data, {"textScale"}, 2.4, 0.2, 20.0)),
        static_cast<float>(ReadClampedNumber(data, {"fontSize"}, 24.0, 8.0, 128.0)),
        ReadBoolean(FindAny(data, {"alwaysFaceCamera"}), false),
        defaults);
}

ri::world::MeasureToolPrimitive BuildMeasureToolPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "measure_tool";
    defaults.type = "measure_tool_primitive";
    defaults.size = {2.0f, 2.0f, 2.0f};

    const std::string_view rawMode = GetStringOrEmpty(FindAny(data, {"mode"}));
    const bool hasStart = FindAny(data, {"start", "pointA"}) != nullptr;
    const bool hasEnd = FindAny(data, {"end", "pointB"}) != nullptr;
    const bool lineMode = rawMode == "line" || rawMode == "distance" || (hasStart && hasEnd);

    const ri::math::Vec3 position = ReadVec3(data, {"position"}, {0.0f, 0.0f, 0.0f});
    const ri::math::Vec3 size = ReadVec3(data, {"size", "scale"}, defaults.size);
    const ri::math::Vec3 start = ReadVec3(data, {"start", "pointA"}, position);
    const ri::math::Vec3 end = ReadVec3(data, {"end", "pointB"}, position + ri::math::Vec3{1.0f, 0.0f, 0.0f});

    ri::world::RuntimeVolumeSeed seed = BuildSeedForPhysicsBoxVolume(data, defaults);
    if (lineMode) {
        seed.position = (start + end) * 0.5f;
        seed.size = ri::math::Vec3{
            std::max(0.1f, std::fabs(end.x - start.x)),
            std::max(0.1f, std::fabs(end.y - start.y)),
            std::max(0.1f, std::fabs(end.z - start.z)),
        };
    } else {
        seed.position = position;
        seed.size = ri::math::Vec3{
            std::max(0.001f, std::fabs(size.x)),
            std::max(0.001f, std::fabs(size.y)),
            std::max(0.001f, std::fabs(size.z)),
        };
    }

    return ri::world::CreateMeasureToolPrimitive(
        seed,
        lineMode ? ri::world::MeasureToolMode::Line : ri::world::MeasureToolMode::Box,
        start,
        end,
        ReadVec3(data, {"labelOffset"}, {0.0f, 0.8f, 0.0f}),
        std::string(GetStringOrEmpty(FindAny(data, {"unitSuffix", "units"}))),
        std::string(GetStringOrEmpty(FindAny(data, {"accentColor"}))),
        std::string(GetStringOrEmpty(FindAny(data, {"backgroundColor"}))),
        std::string(GetStringOrEmpty(FindAny(data, {"textColor"}))),
        static_cast<float>(ReadClampedNumber(data, {"textScale"}, 3.2, 0.5, 24.0)),
        static_cast<float>(ReadClampedNumber(data, {"fontSize"}, 34.0, 14.0, 128.0)),
        ReadBoolean(FindAny(data, {"showWireframe"}), true),
        ReadBoolean(FindAny(data, {"showFill"}), true),
        ReadBoolean(FindAny(data, {"alwaysFaceCamera"}), true),
        defaults);
}

ri::world::RenderTargetSurface BuildRenderTargetSurface(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "render_target_surface";
    defaults.type = "render_target_surface";
    defaults.size = {8.0f, 4.5f, 0.25f};

    const Value* updateEveryFramesValue = FindAny(data, {"updateEveryFrames", "updateFrequency", "updateIntervalFrames"});
    const std::uint32_t updateEveryFrames = updateEveryFramesValue == nullptr
        ? 1U
        : static_cast<std::uint32_t>(ClampFiniteInteger(*updateEveryFramesValue, 1, 1, 120));

    return ri::world::CreateRenderTargetSurface(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ReadVec3(data, {"cameraPosition", "pointA"}, {0.0f, 2.0f, 0.0f}),
        ReadVec3(data, {"cameraLookAt", "pointB"}, {0.0f, 2.0f, -4.0f}),
        static_cast<float>(ReadClampedNumber(data, {"cameraFov", "cameraFovDegrees", "fov"}, 55.0, 25.0, 120.0)),
        FindAny(data, {"renderResolution", "targetResolution"}) == nullptr
            ? 256
            : ClampFiniteInteger(*FindAny(data, {"renderResolution", "targetResolution"}), 256, 64, 1024),
        FindAny(data, {"resolutionCap", "maxResolution"}) == nullptr
            ? 512
            : ClampFiniteInteger(*FindAny(data, {"resolutionCap", "maxResolution"}), 512, 64, 2048),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 20.0, 1.0, 1000.0)),
        updateEveryFrames,
        ReadBoolean(FindAny(data, {"enableDistanceGate"}), true),
        ReadBoolean(FindAny(data, {"editorOnly", "debugOnly"}), false),
        defaults);
}

ri::world::PlanarReflectionSurface BuildPlanarReflectionSurface(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "planar_reflection_surface";
    defaults.type = "planar_reflection_surface";
    defaults.size = {8.0f, 6.0f, 0.25f};

    const Value* updateEveryFramesValue = FindAny(data, {"updateEveryFrames", "updateFrequency", "updateIntervalFrames"});
    const std::uint32_t updateEveryFrames = updateEveryFramesValue == nullptr
        ? 1U
        : static_cast<std::uint32_t>(ClampFiniteInteger(*updateEveryFramesValue, 1, 1, 120));

    return ri::world::CreatePlanarReflectionSurface(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ReadVec3(data, {"planeNormal", "normal"}, {0.0f, 0.0f, 1.0f}),
        static_cast<float>(ReadClampedNumber(data, {"reflectionStrength", "strength"}, 1.0, 0.0, 1.0)),
        FindAny(data, {"renderResolution", "targetResolution"}) == nullptr
            ? 256
            : ClampFiniteInteger(*FindAny(data, {"renderResolution", "targetResolution"}), 256, 64, 1024),
        FindAny(data, {"resolutionCap", "maxResolution"}) == nullptr
            ? 512
            : ClampFiniteInteger(*FindAny(data, {"resolutionCap", "maxResolution"}), 512, 64, 2048),
        static_cast<float>(ReadClampedNumber(data, {"maxActiveDistance"}, 18.0, 1.0, 1000.0)),
        updateEveryFrames,
        ReadBoolean(FindAny(data, {"enableDistanceGate"}), true),
        ReadBoolean(FindAny(data, {"editorOnly", "debugOnly"}), false),
        defaults);
}

ri::world::PassThroughPrimitive BuildPassThroughPrimitive(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "pass_through";
    defaults.type = "pass_through_primitive";
    defaults.size = {2.0f, 2.0f, 2.0f};

    const Value::Object* materialData = nullptr;
    if (const Value* materialValue = FindAny(data, {"material", "renderMaterial"}); materialValue != nullptr) {
        materialData = materialValue->TryGetObject();
    }

    const Value::Object* visualBehaviorData = nullptr;
    if (const Value* visualBehaviorValue = FindAny(data, {"visualBehavior", "visuals"}); visualBehaviorValue != nullptr) {
        visualBehaviorData = visualBehaviorValue->TryGetObject();
    }

    const Value::Object* interactionProfileData = nullptr;
    if (const Value* interactionProfileValue = FindAny(data, {"interactionProfile", "interaction"}); interactionProfileValue != nullptr) {
        interactionProfileData = interactionProfileValue->TryGetObject();
    }

    const Value::Object* eventsData = nullptr;
    if (const Value* eventsValue = FindAny(data, {"events"}); eventsValue != nullptr) {
        eventsData = eventsValue->TryGetObject();
    }

    const Value::Object* debugData = nullptr;
    if (const Value* debugValue = FindAny(data, {"debug"}); debugValue != nullptr) {
        debugData = debugValue->TryGetObject();
    }

    ri::world::PassThroughMaterialSettings material{};
    material.baseColor = std::string(GetStringOrEmpty(FindAnyInObject(materialData, {"baseColor", "color"})));
    material.opacity = static_cast<float>(ReadClampedNumber(
        materialData == nullptr ? Value::Object{} : *materialData,
        {"opacity"},
        0.35,
        0.0,
        1.0));
    material.emissiveColor = std::string(GetStringOrEmpty(FindAnyInObject(materialData, {"emissiveColor"})));
    material.emissiveIntensity = static_cast<float>(ReadClampedNumber(
        materialData == nullptr ? Value::Object{} : *materialData,
        {"emissiveIntensity"},
        0.15,
        0.0,
        16.0));
    material.doubleSided = ReadBoolean(FindAnyInObject(materialData, {"doubleSided"}), true);
    material.depthWrite = ReadBoolean(FindAnyInObject(materialData, {"depthWrite"}), false);
    material.depthTest = ReadBoolean(FindAnyInObject(materialData, {"depthTest"}), true);
    if (const std::optional<ri::world::PassThroughBlendMode> blendMode =
            ParsePassThroughBlendMode(FindAnyInObject(materialData, {"blendMode"}));
        blendMode.has_value()) {
        material.blendMode = *blendMode;
    }

    ri::world::PassThroughVisualBehavior visualBehavior{};
    visualBehavior.pulseEnabled = ReadBoolean(FindAnyInObject(visualBehaviorData, {"pulseEnabled"}), false);
    visualBehavior.pulseSpeed = static_cast<float>(ReadClampedNumber(
        visualBehaviorData == nullptr ? Value::Object{} : *visualBehaviorData,
        {"pulseSpeed"},
        1.2,
        0.0,
        24.0));
    visualBehavior.pulseMinOpacity = static_cast<float>(ReadClampedNumber(
        visualBehaviorData == nullptr ? Value::Object{} : *visualBehaviorData,
        {"pulseMinOpacity"},
        0.20,
        0.0,
        1.0));
    visualBehavior.pulseMaxOpacity = static_cast<float>(ReadClampedNumber(
        visualBehaviorData == nullptr ? Value::Object{} : *visualBehaviorData,
        {"pulseMaxOpacity"},
        0.45,
        0.0,
        1.0));
    visualBehavior.distanceFadeEnabled = ReadBoolean(FindAnyInObject(visualBehaviorData, {"distanceFadeEnabled"}), false);
    visualBehavior.fadeNear = static_cast<float>(ReadClampedNumber(
        visualBehaviorData == nullptr ? Value::Object{} : *visualBehaviorData,
        {"fadeNear"},
        1.0,
        0.0,
        10000.0));
    visualBehavior.fadeFar = static_cast<float>(ReadClampedNumber(
        visualBehaviorData == nullptr ? Value::Object{} : *visualBehaviorData,
        {"fadeFar"},
        20.0,
        0.0,
        10000.0));
    visualBehavior.rimHighlightEnabled = ReadBoolean(FindAnyInObject(visualBehaviorData, {"rimHighlightEnabled"}), false);
    visualBehavior.rimPower = static_cast<float>(ReadClampedNumber(
        visualBehaviorData == nullptr ? Value::Object{} : *visualBehaviorData,
        {"rimPower"},
        2.0,
        0.1,
        16.0));

    ri::world::PassThroughInteractionProfile interactionProfile{};
    interactionProfile.blocksPlayer = ReadBoolean(FindAnyInObject(interactionProfileData, {"blocksPlayer"}), false);
    interactionProfile.blocksNpc = ReadBoolean(FindAnyInObject(interactionProfileData, {"blocksNPC", "blocksNpc"}), false);
    interactionProfile.blocksProjectiles = ReadBoolean(FindAnyInObject(interactionProfileData, {"blocksProjectiles"}), false);
    interactionProfile.affectsNavigation = ReadBoolean(FindAnyInObject(interactionProfileData, {"affectsNavigation"}), false);
    interactionProfile.raycastSelectable = ReadBoolean(FindAnyInObject(interactionProfileData, {"raycastSelectable"}), true);

    ri::world::PassThroughEventHooks events{};
    events.onEnter = std::string(GetStringOrEmpty(FindAnyInObject(eventsData, {"onEnter"})));
    events.onExit = std::string(GetStringOrEmpty(FindAnyInObject(eventsData, {"onExit"})));
    events.onUse = std::string(GetStringOrEmpty(FindAnyInObject(eventsData, {"onUse"})));

    ri::world::PassThroughDebugSettings debug{};
    debug.label = std::string(GetStringOrEmpty(FindAnyInObject(debugData, {"label"})));
    debug.showBounds = ReadBoolean(FindAnyInObject(debugData, {"showBounds"}), false);

    ri::world::PassThroughPrimitiveShape primitiveShape = ri::world::PassThroughPrimitiveShape::Box;
    if (const std::optional<ri::world::PassThroughPrimitiveShape> parsedShape =
            ParsePassThroughPrimitiveShape(FindAny(data, {"shape", "primitiveType"}));
        parsedShape.has_value()) {
        primitiveShape = *parsedShape;
    }

    const std::string customMeshAsset = std::string(GetStringOrEmpty(FindAny(data, {"meshAsset", "meshAssetId", "mesh"})));

    return ri::world::CreatePassThroughPrimitive(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        primitiveShape,
        customMeshAsset,
        material,
        visualBehavior,
        interactionProfile,
        events,
        debug,
        defaults);
}

ri::world::SkyProjectionSurface BuildSkyProjectionSurface(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "sky_projection_surface";
    defaults.type = "sky_projection_surface";
    defaults.size = {16.0f, 8.0f, 1.0f};

    const Value::Object* visualData = nullptr;
    if (const Value* visualValue = FindAny(data, {"visual"}); visualValue != nullptr) {
        visualData = visualValue->TryGetObject();
    }

    const Value::Object* behaviorData = nullptr;
    if (const Value* behaviorValue = FindAny(data, {"behavior"}); behaviorValue != nullptr) {
        behaviorData = behaviorValue->TryGetObject();
    }

    const Value::Object* debugData = nullptr;
    if (const Value* debugValue = FindAny(data, {"debug"}); debugValue != nullptr) {
        debugData = debugValue->TryGetObject();
    }

    ri::world::SkyProjectionVisualSettings visual{};
    if (const std::optional<ri::world::SkyProjectionVisualMode> mode =
            ParseSkyProjectionVisualMode(FindAnyInObject(visualData, {"mode"}));
        mode.has_value()) {
        visual.mode = *mode;
    }
    visual.color = std::string(GetStringOrEmpty(FindAnyInObject(visualData, {"color", "baseColor"})));
    visual.topColor = std::string(GetStringOrEmpty(FindAnyInObject(visualData, {"topColor"})));
    visual.bottomColor = std::string(GetStringOrEmpty(FindAnyInObject(visualData, {"bottomColor"})));
    visual.textureId = std::string(GetStringOrEmpty(FindAnyInObject(visualData, {"textureId", "texture", "imageId"})));
    visual.opacity = static_cast<float>(ReadClampedNumber(
        visualData == nullptr ? Value::Object{} : *visualData,
        {"opacity"},
        1.0,
        0.0,
        1.0));
    visual.doubleSided = ReadBoolean(FindAnyInObject(visualData, {"doubleSided"}), true);
    visual.unlit = ReadBoolean(FindAnyInObject(visualData, {"unlit"}), true);

    ri::world::SkyProjectionBehaviorSettings behavior{};
    behavior.followCameraYaw = ReadBoolean(FindAnyInObject(behaviorData, {"followCameraYaw"}), false);
    behavior.parallaxFactor = static_cast<float>(ReadClampedNumber(
        behaviorData == nullptr ? Value::Object{} : *behaviorData,
        {"parallaxFactor"},
        0.0,
        0.0,
        1.0));
    behavior.distanceLock = ReadBoolean(FindAnyInObject(behaviorData, {"distanceLock"}), false);
    behavior.depthWrite = ReadBoolean(FindAnyInObject(behaviorData, {"depthWrite"}), false);
    if (const std::optional<ri::world::SkyProjectionRenderLayer> layer =
            ParseSkyProjectionRenderLayer(FindAnyInObject(behaviorData, {"renderLayer"}));
        layer.has_value()) {
        behavior.renderLayer = *layer;
    }

    ri::world::SkyProjectionDebugSettings debug{};
    debug.label = std::string(GetStringOrEmpty(FindAnyInObject(debugData, {"label"})));
    debug.showBounds = ReadBoolean(FindAnyInObject(debugData, {"showBounds"}), false);

    return ri::world::CreateSkyProjectionSurface(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        std::string(GetStringOrEmpty(FindAny(data, {"primitiveType", "shape"}))),
        visual,
        behavior,
        debug,
        defaults);
}

[[nodiscard]] ri::world::InfoPanelValue ParseInfoPanelValue(const Value* value) {
    if (value == nullptr) {
        return ri::world::InfoPanelValue{};
    }
    if (const std::string* text = value->TryGetString(); text != nullptr) {
        return *text;
    }
    if (const double* number = value->TryGetNumber(); number != nullptr && std::isfinite(*number)) {
        return *number;
    }
    if (const bool* flag = value->TryGetBoolean(); flag != nullptr) {
        return *flag;
    }
    return ri::world::InfoPanelValue{};
}

ri::world::DynamicInfoPanelSpawner BuildDynamicInfoPanelSpawner(const Value::Object& data) {
    ri::world::DynamicInfoPanelSpawner spawner{};
    const ri::world::RuntimeVolumeSeed seed = BuildSeedForPhysicsBoxVolume(data, {
        .runtimeId = "info_panel",
        .type = "info_panel",
        .size = {2.2f, 1.25f, 0.08f},
    });
    spawner.id = seed.id;
    spawner.position = seed.position.value_or(spawner.position);
    spawner.size = seed.size.value_or(spawner.size);
    spawner.title = std::string(GetStringOrEmpty(FindAny(data, {"title", "name"})));
    spawner.refreshIntervalSeconds = std::max(
        0.01,
        ReadClampedNumber(data, {"refreshIntervalSeconds", "refreshSeconds", "refreshRateSeconds"}, 0.25, 0.01, 10.0));
    spawner.focusable = ReadBoolean(FindAny(data, {"focusable"}), true);
    spawner.interactionPrompt = std::string(GetStringOrEmpty(FindAny(data, {"interactionPrompt", "prompt", "usePrompt"})));
    if (spawner.interactionPrompt.empty()) {
        spawner.interactionPrompt = "Read Panel";
    }
    spawner.interactionHook = std::string(GetStringOrEmpty(FindAny(data, {"interactionHook", "onInteract"})));

    if (const Value* text = FindAny(data, {"text", "interactText"}); text != nullptr) {
        if (const std::string* str = text->TryGetString(); str != nullptr) {
            spawner.panel.text = *str;
        }
    }
    if (const Value* lines = FindAny(data, {"lines"}); lines != nullptr) {
        if (const Value::Array* arr = lines->TryGetArray(); arr != nullptr) {
            for (const Value& line : *arr) {
                if (const std::string* text = line.TryGetString(); text != nullptr && !text->empty()) {
                    spawner.panel.lines.push_back(*text);
                }
            }
        }
    }

    const std::string_view mode = GetStringOrEmpty(FindAny(data, {"bindingsMode"}));
    spawner.panel.replaceBindings = mode == "replace";
    if (const Value* bindings = FindAny(data, {"bindings"}); bindings != nullptr) {
        if (const Value::Array* entries = bindings->TryGetArray(); entries != nullptr) {
            spawner.panel.bindings.reserve(entries->size());
            for (const Value& entry : *entries) {
                const Value::Object* object = entry.TryGetObject();
                if (object == nullptr) {
                    continue;
                }
                ri::world::InfoPanelBinding binding{};
                binding.label = std::string(GetStringOrEmpty(FindAny(*object, {"label"})));
                binding.logicEntityId = std::string(GetStringOrEmpty(FindAny(*object, {"logicEntityId", "entityId"})));
                binding.property = std::string(GetStringOrEmpty(FindAny(*object, {"property"})));
                binding.worldValue = std::string(GetStringOrEmpty(FindAny(*object, {"worldValue"})));
                binding.worldFlag = std::string(GetStringOrEmpty(FindAny(*object, {"worldFlag"})));
                binding.runtimeMetric = std::string(GetStringOrEmpty(FindAny(*object, {"runtimeMetric", "metric"})));
                binding.value = ParseInfoPanelValue(FindAny(*object, {"value"}));
                binding.fallback = ParseInfoPanelValue(FindAny(*object, {"fallback"}));
                spawner.panel.bindings.push_back(std::move(binding));
            }
        }
    }
    return spawner;
}

struct ParsedVolumetricEmitterLayers {
    ri::world::VolumetricEmitterEmissionSettings emission{};
    ri::world::VolumetricEmitterParticleSettings particle{};
    ri::world::VolumetricEmitterRenderSettings render{};
    ri::world::VolumetricEmitterCullingSettings culling{};
    ri::world::VolumetricEmitterDebugSettings debug{};
};

[[nodiscard]] ParsedVolumetricEmitterLayers ParseVolumetricEmitterLayers(const Value::Object& data) {
    const Value::Object* emissionData = nullptr;
    if (const Value* value = FindAny(data, {"emission"}); value != nullptr) {
        emissionData = value->TryGetObject();
    }
    const Value::Object* particleData = nullptr;
    if (const Value* value = FindAny(data, {"particle"}); value != nullptr) {
        particleData = value->TryGetObject();
    }
    const Value::Object* renderData = nullptr;
    if (const Value* value = FindAny(data, {"render"}); value != nullptr) {
        renderData = value->TryGetObject();
    }
    const Value::Object* cullingData = nullptr;
    if (const Value* value = FindAny(data, {"culling"}); value != nullptr) {
        cullingData = value->TryGetObject();
    }
    const Value::Object* debugData = nullptr;
    if (const Value* value = FindAny(data, {"debug"}); value != nullptr) {
        debugData = value->TryGetObject();
    }

    ParsedVolumetricEmitterLayers out{};
    if (const Value* particleCountValue = FindAnyInObject(emissionData, {"particleCount"});
        particleCountValue != nullptr) {
        out.emission.particleCount =
            static_cast<std::uint32_t>(ClampFiniteInteger(*particleCountValue, 96, 8, 2048));
    }
    if (const std::optional<ri::world::VolumetricEmitterSpawnMode> spawnMode =
            ParseVolumetricEmitterSpawnMode(FindAnyInObject(emissionData, {"spawnMode"}));
        spawnMode.has_value()) {
        out.emission.spawnMode = *spawnMode;
    }
    if (const Value* lifetime = FindAnyInObject(emissionData, {"lifetimeSeconds"}); lifetime != nullptr) {
        if (const Value::Array* range = lifetime->TryGetArray(); range != nullptr && !range->empty()) {
            const Value* minValue = &range->front();
            const Value* maxValue = range->size() > 1U ? &(*range)[1] : &range->front();
            const float minSeconds = static_cast<float>(ClampFiniteNumber(*minValue, 2.0, 0.01, 120.0));
            const float maxSeconds = static_cast<float>(ClampFiniteNumber(*maxValue, 6.0, 0.01, 120.0));
            out.emission.lifetimeMinSeconds = std::min(minSeconds, maxSeconds);
            out.emission.lifetimeMaxSeconds = std::max(minSeconds, maxSeconds);
        }
    }
    out.emission.spawnRatePerSecond = static_cast<float>(ReadClampedNumber(
        emissionData == nullptr ? Value::Object{} : *emissionData,
        {"spawnRatePerSecond"},
        0.0,
        0.0,
        4096.0));
    out.emission.loop = ReadBoolean(FindAnyInObject(emissionData, {"loop"}), true);

    out.particle.size = static_cast<float>(ReadClampedNumber(
        particleData == nullptr ? Value::Object{} : *particleData,
        {"size"},
        0.08,
        0.001,
        10.0));
    out.particle.sizeJitter = static_cast<float>(ReadClampedNumber(
        particleData == nullptr ? Value::Object{} : *particleData,
        {"sizeJitter"},
        0.35,
        0.0,
        5.0));
    out.particle.color = std::string(GetStringOrEmpty(FindAnyInObject(particleData, {"color"})));
    out.particle.opacity = static_cast<float>(ReadClampedNumber(
        particleData == nullptr ? Value::Object{} : *particleData,
        {"opacity"},
        0.18,
        0.0,
        1.0));
    out.particle.velocity =
        ReadVec3(particleData == nullptr ? Value::Object{} : *particleData, {"velocity"}, {0.0f, 0.04f, 0.0f});
    out.particle.velocityJitter = ReadVec3(
        particleData == nullptr ? Value::Object{} : *particleData,
        {"velocityJitter"},
        {0.02f, 0.03f, 0.02f});
    out.particle.softFade = ReadBoolean(FindAnyInObject(particleData, {"softFade"}), true);

    if (const std::optional<ri::world::VolumetricEmitterBlendMode> blendMode =
            ParseVolumetricEmitterBlendMode(FindAnyInObject(renderData, {"blendMode"}));
        blendMode.has_value()) {
        out.render.blendMode = *blendMode;
    }
    out.render.depthWrite = ReadBoolean(FindAnyInObject(renderData, {"depthWrite"}), false);
    out.render.depthTest = ReadBoolean(FindAnyInObject(renderData, {"depthTest"}), true);
    out.render.billboard = ReadBoolean(FindAnyInObject(renderData, {"billboard"}), true);
    out.render.sortMode = ri::world::VolumetricEmitterSortMode::PerEmitter;

    out.culling.maxActiveDistance = static_cast<float>(ReadClampedNumber(
        cullingData == nullptr ? Value::Object{} : *cullingData,
        {"maxActiveDistance"},
        40.0,
        1.0,
        10000.0));
    out.culling.frustumCulling = ReadBoolean(FindAnyInObject(cullingData, {"frustumCulling"}), true);
    out.culling.pauseWhenOffscreen = ReadBoolean(FindAnyInObject(cullingData, {"pauseWhenOffscreen"}), true);

    out.debug.showBounds = ReadBoolean(FindAnyInObject(debugData, {"showBounds"}), false);
    out.debug.showSpawnPoints = ReadBoolean(FindAnyInObject(debugData, {"showSpawnPoints"}), false);
    out.debug.label = std::string(GetStringOrEmpty(FindAnyInObject(debugData, {"label"})));
    return out;
}

[[nodiscard]] ri::world::ParticleSpawnAuthoring ParseParticleSpawnAuthoringFromData(const Value::Object& data) {
    const Value* wrap = FindAny(data, {"particleSpawn"});
    const Value::Object* wrapObj = wrap != nullptr ? wrap->TryGetObject() : nullptr;
    const Value::Object& o = wrapObj != nullptr ? *wrapObj : data;

    ri::world::ParticleSpawnAuthoring spawn{};
    spawn.displayName = std::string(GetStringOrEmpty(FindAny(o, {"displayName", "title"})));
    spawn.particleSystemPresetId =
        std::string(GetStringOrEmpty(FindAny(o, {"particleSystemPresetId", "presetId", "systemId", "particlePreset"})));
    spawn.meshAssetId = std::string(GetStringOrEmpty(FindAny(o, {"meshAssetId", "meshId"})));
    spawn.materialAssetId = std::string(GetStringOrEmpty(FindAny(o, {"materialAssetId", "materialId"})));
    spawn.worldCollision = ReadBoolean(FindAny(o, {"worldCollision", "collideWithWorld"}), false);

    const Value::Object* activation = nullptr;
    if (const Value* v = FindAny(o, {"activation"}); v != nullptr) {
        activation = v->TryGetObject();
    }
    if (activation != nullptr) {
        spawn.activation.outerProximityRadius = static_cast<float>(ReadClampedNumber(
            *activation,
            {"outerProximityRadius", "outerRadius", "proximityRadius"},
            0.0,
            0.0,
            100000.0));
        spawn.activation.strictInnerVolumeOnly =
            ReadBoolean(FindAnyInObject(activation, {"strictInnerVolumeOnly", "strictInner"}), false);
        spawn.activation.alwaysOnAmbient =
            ReadBoolean(FindAnyInObject(activation, {"alwaysOnAmbient", "alwaysOn"}), false);
    }

    const Value::Object* emissionPolicy = nullptr;
    if (const Value* v = FindAny(o, {"emissionPolicy"}); v != nullptr) {
        emissionPolicy = v->TryGetObject();
    }
    if (emissionPolicy != nullptr) {
        if (const Value* burst = FindAnyInObject(emissionPolicy, {"burstCountOnEnter", "burstOnEnter"});
            burst != nullptr) {
            spawn.emissionPolicy.burstCountOnEnter =
                static_cast<std::uint32_t>(ClampFiniteInteger(*burst, 0, 0, 100000));
        }
        spawn.emissionPolicy.oneShot = ReadBoolean(FindAnyInObject(emissionPolicy, {"oneShot"}), false);
    }

    const Value::Object* budget = nullptr;
    if (const Value* v = FindAny(o, {"budget", "lod"}); v != nullptr) {
        budget = v->TryGetObject();
    }
    if (budget != nullptr) {
        if (const Value* cost = FindAnyInObject(budget, {"maxOnScreenCostHint", "maxCost"}); cost != nullptr) {
            spawn.budget.maxOnScreenCostHint =
                static_cast<std::uint32_t>(ClampFiniteInteger(*cost, 0, 0, 1000000));
        }
        if (const Value* tier = FindAnyInObject(budget, {"disableAtOrBelowQualityTier", "maxQualityTier"});
            tier != nullptr) {
            const int t = static_cast<int>(ClampFiniteInteger(*tier, 0, 0, 255));
            spawn.budget.disableAtOrBelowQualityTier = static_cast<std::uint8_t>(t);
        }
    }

    const Value::Object* binding = nullptr;
    if (const Value* v = FindAny(o, {"binding"}); v != nullptr) {
        binding = v->TryGetObject();
    }
    if (binding != nullptr) {
        spawn.binding.followNodeId = std::string(GetStringOrEmpty(FindAnyInObject(binding, {"followNodeId", "nodeId"})));
        spawn.binding.followSocketName =
            std::string(GetStringOrEmpty(FindAnyInObject(binding, {"followSocketName", "socket"})));
    }

    const Value::Object* environment = nullptr;
    if (const Value* v = FindAny(o, {"environment"}); v != nullptr) {
        environment = v->TryGetObject();
    }
    if (environment != nullptr) {
        spawn.environment.applyGlobalWind = ReadBoolean(FindAnyInObject(environment, {"applyGlobalWind"}), false);
        spawn.environment.localWindFieldVolumeId =
            std::string(GetStringOrEmpty(FindAnyInObject(environment, {"localWindFieldVolumeId", "windVolumeId"})));
        spawn.environment.reduceWhenOccluded =
            ReadBoolean(FindAnyInObject(environment, {"reduceWhenOccluded"}), false);
        spawn.environment.reduceWhenIndoor =
            ReadBoolean(FindAnyInObject(environment, {"reduceWhenIndoor"}), false);
    }

    return spawn;
}

ri::world::VolumetricEmitterBounds BuildVolumetricEmitterBounds(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "volumetric_emitter";
    defaults.type = "volumetric_emitter_bounds";
    defaults.size = {8.0f, 5.0f, 8.0f};

    const ParsedVolumetricEmitterLayers layers = ParseVolumetricEmitterLayers(data);
    return ri::world::CreateVolumetricEmitterBounds(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        layers.emission,
        layers.particle,
        layers.render,
        layers.culling,
        layers.debug,
        defaults);
}

ri::world::VolumetricEmitterBounds BuildParticleSpawnVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "particle_spawn";
    defaults.type = "particle_spawn_volume";
    defaults.size = {8.0f, 5.0f, 8.0f};

    const ParsedVolumetricEmitterLayers layers = ParseVolumetricEmitterLayers(data);
    const ri::world::ParticleSpawnAuthoring spawn = ParseParticleSpawnAuthoringFromData(data);
    return ri::world::CreateVolumetricEmitterBounds(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        layers.emission,
        layers.particle,
        layers.render,
        layers.culling,
        layers.debug,
        defaults,
        std::make_optional(spawn));
}

ri::world::OcclusionPortalVolume BuildOcclusionPortalVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "occlusion_portal";
    defaults.type = "occlusion_portal";
    defaults.size = {4.0f, 4.0f, 0.18f};
    return ri::world::CreateOcclusionPortalVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        ReadBoolean(FindAny(data, {"isClosed"}), true),
        defaults);
}

ri::world::PostProcessVolume BuildPostProcessVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "post_process_volume";
    defaults.type = "post_process_volume";
    defaults.shape = ri::world::VolumeShape::Sphere;
    defaults.size = {4.0f, 4.0f, 4.0f};
    ri::world::PostProcessVolume volume = ri::world::CreatePostProcessVolume(
        BuildSeedForEnvironmentSphereVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"tintStrength"}, 0.12, 0.0, 1.0)),
        static_cast<float>(ReadClampedNumber(data, {"blurAmount"}, 0.0012, 0.0, 0.02)),
        static_cast<float>(ReadClampedNumber(data, {"noiseAmount"}, 0.003, 0.0, 0.2)),
        static_cast<float>(ReadClampedNumber(data, {"scanlineAmount"}, 0.0015, 0.0, 0.08)),
        static_cast<float>(ReadClampedNumber(data, {"barrelDistortion"}, 0.003, 0.0, 0.1)),
        static_cast<float>(ReadClampedNumber(data, {"chromaticAberration"}, 0.00025, 0.0, 0.02)),
        defaults);
    volume.tintColor = ReadColor(data, {"tintColor"}, {0.624f, 1.000f, 0.616f});
    return volume;
}

ri::world::AudioReverbVolume BuildAudioReverbVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "audio_reverb_volume";
    defaults.type = "audio_reverb_volume";
    defaults.shape = ri::world::VolumeShape::Sphere;
    defaults.size = {4.0f, 4.0f, 4.0f};
    return ri::world::CreateAudioReverbVolume(
        BuildSeedForEnvironmentSphereVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"reverbMix"}, 0.55, 0.0, 1.0)),
        static_cast<float>(ReadClampedNumber(data, {"echoDelayMs"}, 160.0, 0.0, 2000.0)),
        static_cast<float>(ReadClampedNumber(data, {"echoFeedback"}, 0.42, 0.0, 0.95)),
        static_cast<float>(ReadClampedNumber(data, {"dampening"}, 0.08, 0.0, 1.0)),
        static_cast<float>(ReadClampedNumber(data, {"volumeScale"}, 1.0, 0.2, 2.0)),
        static_cast<float>(ReadClampedNumber(data, {"playbackRate"}, 1.0, 0.5, 1.5)),
        defaults);
}

ri::world::AudioOcclusionVolume BuildAudioOcclusionVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "audio_occlusion";
    defaults.type = "audio_occlusion_volume";
    defaults.size = {5.0f, 4.0f, 5.0f};
    ri::world::RuntimeVolumeSeed seed = BuildSeedWithSizeAliases(data, defaults);
    ClampMinimumExtents(seed);
    return ri::world::CreateAudioOcclusionVolume(
        seed,
        static_cast<float>(ReadClampedNumber(data, {"occlusionStrength", "dampening"}, 0.45, 0.0, 1.0)),
        static_cast<float>(ReadClampedNumber(data, {"volumeScale"}, 0.78, 0.1, 1.0)),
        defaults);
}

ri::world::AmbientAudioVolume BuildAmbientAudioVolume(const Value::Object& data) {
    const std::string_view rawType = GetStringOrEmpty(FindAny(data, {"type"}));

    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "ambient_audio";
    defaults.type = rawType == "ambient_audio_spline" ? "ambient_audio_spline" : "ambient_audio_volume";
    defaults.size = {8.0f, 4.0f, 8.0f};

    std::vector<ri::math::Vec3> splinePoints = ParseVec3List(FindAny(data, {"spline"}));
    if (splinePoints.size() < 2U) {
        splinePoints.clear();
    }

    const std::string audioPath = [] (const Value::Object& source) {
        const std::string_view path = GetStringOrEmpty(FindAny(source, {"audioPath", "soundPath", "filePath", "src"}));
        return std::string(path);
    }(data);

    std::string label = std::string(GetStringOrEmpty(FindAny(data, {"label", "id"})));
    if (label.empty()) {
        label = "ambient_audio";
    }

    return ri::world::CreateAmbientAudioVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        audioPath,
        static_cast<float>(ReadClampedNumber(data, {"volume", "baseVolume"}, 0.35, 0.0, 1.0)),
        static_cast<float>(ReadClampedNumber(
            data,
            {"maxDistance", "radius"},
            defaults.size.x,
            0.5,
            256.0)),
        std::move(label),
        std::move(splinePoints),
        defaults);
}

ri::world::GenericTriggerVolume BuildGenericTriggerVolume(const Value::Object& data) {
    const std::string_view rawType = GetStringOrEmpty(FindAny(data, {"type"}));

    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "trigger_volume";
    defaults.type = rawType.empty() ? std::string("trigger_volume") : std::string(rawType);
    defaults.shape = ri::world::VolumeShape::Sphere;
    defaults.size = {2.0f, 2.0f, 2.0f};

    ri::world::RuntimeVolumeSeed seed = BuildSeedWithSizeAliases(data, defaults);
    ClampMinimumExtents(seed);
    if (!seed.radius.has_value() && seed.shape.value_or(defaults.shape) == ri::world::VolumeShape::Sphere) {
        seed.radius = 1.5f;
    }

    if (const Value* armedValue = FindAny(data, {"startArmed", "armed"}); armedValue != nullptr) {
        seed.startArmed = ReadBoolean(armedValue, true);
    }

    ri::world::GenericTriggerVolume volume = ri::world::CreateGenericTriggerVolume(
        seed,
        ReadClampedNumber(data, {"stayFrequency", "broadcastFrequency"}, 0.0, 0.0, 3600.0),
        defaults);
    volume.onEnterEvent = std::string(GetStringOrEmpty(FindAny(data, {"onEnterEvent", "enterEvent"})));
    volume.onStayEvent = std::string(GetStringOrEmpty(FindAny(data, {"onStayEvent", "stayEvent"})));
    volume.onExitEvent = std::string(GetStringOrEmpty(FindAny(data, {"onExitEvent", "exitEvent"})));
    return volume;
}

ri::world::SpatialQueryVolume BuildSpatialQueryVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "query_volume";
    defaults.type = "spatial_query_volume";
    defaults.shape = ri::world::VolumeShape::Sphere;
    defaults.size = {2.0f, 2.0f, 2.0f};

    ri::world::RuntimeVolumeSeed seed = BuildSeedWithSizeAliases(data, defaults);
    ClampMinimumExtents(seed);

    const Value* filterMaskValue = FindAny(data, {"filterMask"});
    const std::uint32_t filterMask = filterMaskValue == nullptr
        ? 0U
        : static_cast<std::uint32_t>(ClampFiniteInteger(*filterMaskValue, 0, 0, 2147483647));

    ri::world::SpatialQueryVolume volume = ri::world::CreateSpatialQueryVolume(
        seed,
        ReadClampedNumber(data, {"broadcastFrequency"}, 0.0, 0.0, 3600.0),
        filterMask,
        defaults);
    volume.onEnterEvent = std::string(GetStringOrEmpty(FindAny(data, {"onEnterEvent", "enterEvent"})));
    volume.onStayEvent = std::string(GetStringOrEmpty(FindAny(data, {"onStayEvent", "stayEvent"})));
    volume.onExitEvent = std::string(GetStringOrEmpty(FindAny(data, {"onExitEvent", "exitEvent"})));
    return volume;
}

ri::world::StreamingLevelVolume BuildStreamingLevelVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "streaming_level";
    defaults.type = "streaming_level_volume";
    defaults.size = {6.0f, 4.0f, 6.0f};
    return ri::world::CreateStreamingLevelVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        std::string(GetStringOrEmpty(FindAny(data, {"targetLevel", "level"}))),
        defaults);
}

ri::world::CheckpointSpawnVolume BuildCheckpointSpawnVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "checkpoint_spawn";
    defaults.type = "checkpoint_spawn_volume";
    defaults.size = {3.0f, 3.0f, 3.0f};
    const ri::math::Vec3 position = ReadVec3(data, {"position"}, {0.0f, 0.0f, 0.0f});
    return ri::world::CreateCheckpointSpawnVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        std::string(GetStringOrEmpty(FindAny(data, {"targetLevel", "level"}))),
        ReadVec3(data, {"respawn", "pointA", "position"}, position),
        ReadVec3(data, {"respawnRotation", "rotation"}, {0.0f, 0.0f, 0.0f}),
        defaults);
}

ri::world::TeleportVolume BuildTeleportVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "teleport";
    defaults.type = "teleport_volume";
    defaults.size = {4.0f, 4.0f, 4.0f};

    std::string targetId = std::string(GetStringOrEmpty(FindAny(data, {"targetId", "anchorId", "destinationId"})));
    if (targetId.empty()) {
        const std::vector<std::string> targetIds = ParseStringList(FindAny(data, {"targetIds"}));
        for (const std::string& candidate : targetIds) {
            if (!candidate.empty()) {
                targetId = candidate;
                break;
            }
        }
    }

    const ri::math::Vec3 position = ReadVec3(data, {"position"}, {0.0f, 0.0f, 0.0f});
    return ri::world::CreateTeleportVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        std::move(targetId),
        ReadVec3(data, {"targetPosition", "destination", "pointB"}, position),
        ReadVec3(data, {"targetRotation", "respawnRotation", "rotation"}, {0.0f, 0.0f, 0.0f}),
        ReadVec3(data, {"offset"}, {0.0f, 0.0f, 0.0f}),
        defaults);
}

ri::world::LaunchVolume BuildLaunchVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "launch";
    defaults.type = "launch_volume";
    defaults.size = {4.0f, 4.0f, 4.0f};

    ri::math::Vec3 impulse = ReadVec3(data, {"impulse", "forceDirection", "flowDirection"}, {0.0f, 8.0f, 0.0f});
    if (ri::math::LengthSquared(impulse) <= 0.000001f) {
        impulse = {0.0f, static_cast<float>(ReadClampedNumber(data, {"force", "strength"}, 10.0, -60.0, 60.0)), 0.0f};
    } else {
        const float strength = static_cast<float>(ReadClampedNumber(
            data,
            {"force", "strength"},
            ri::math::Length(impulse),
            -120.0,
            120.0));
        impulse = NormalizeOrFallback(impulse, {0.0f, 1.0f, 0.0f}) * strength;
    }

    return ri::world::CreateLaunchVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        impulse,
        ReadBoolean(FindAny(data, {"affectPhysics"}), true),
        defaults);
}

ri::world::AnalyticsHeatmapVolume BuildAnalyticsHeatmapVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "analytics_heatmap";
    defaults.type = "analytics_heatmap_volume";
    defaults.size = {6.0f, 4.0f, 6.0f};
    ri::world::AnalyticsHeatmapVolume volume = ri::world::CreateAnalyticsHeatmapVolume(
        BuildSeedForPhysicsBoxVolume(data, defaults),
        defaults);
    volume.broadcastFrequency = ReadClampedNumber(
        data,
        {"stayIntervalSeconds", "broadcastFrequency", "stayInterval"},
        0.0,
        0.0,
        3600.0);
    volume.sampleSubjectMask = static_cast<std::uint32_t>(ReadClampedNumber(
        data,
        {"sampleSubjectMask", "subjectMask"},
        static_cast<double>(ri::world::kAnalyticsHeatmapSamplePlayer),
        0.0,
        static_cast<double>(std::numeric_limits<std::uint32_t>::max())));
    volume.debugDraw =
        ReadBoolean(FindAny(data, {"debugDraw", "showInEditor", "heatmapDebug"}), false);
    return volume;
}

ri::world::LocalizedFogVolume BuildLocalizedFogVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "localized_fog";
    defaults.type = "localized_fog_volume";
    defaults.size = {6.0f, 4.0f, 6.0f};
    const double tintStrength = ReadClampedNumber(data, {"tintStrength"}, 0.12, 0.0, 1.0);
    const double blurAmount = ReadClampedNumber(data, {"blurAmount"}, 0.0016, 0.0, 0.02);
    ri::world::RuntimeVolumeSeed seed = BuildSeedWithSizeAliases(data, defaults);
    ClampMinimumExtents(seed);
    ri::world::LocalizedFogVolume volume = ri::world::CreateLocalizedFogVolume(
        seed,
        static_cast<float>(tintStrength),
        static_cast<float>(blurAmount),
        defaults);
    volume.tintColor = ReadColor(data, {"tintColor", "color"}, {0.725f, 0.780f, 0.843f});
    return volume;
}

ri::world::FogBlockerVolume BuildVolumetricFogBlocker(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "fog_blocker";
    defaults.type = "volumetric_fog_blocker";
    defaults.size = {4.0f, 4.0f, 4.0f};
    ri::world::RuntimeVolumeSeed seed = BuildSeedWithSizeAliases(data, defaults);
    ClampMinimumExtents(seed);
    return ri::world::CreateFogBlockerVolume(seed, defaults);
}

ri::world::FluidSimulationVolume BuildFluidSimulationVolume(const Value::Object& data) {
    ri::world::VolumeDefaults defaults{};
    defaults.runtimeId = "fluid_volume";
    defaults.type = "fluid_simulation_volume";
    defaults.size = {8.0f, 4.0f, 8.0f};

    const ri::math::Vec3 flowDirection = ReadVec3(data, {"flowDirection", "forceDirection"}, {0.0f, 0.0f, 0.0f});
    const float flowStrength = static_cast<float>(ReadClampedNumber(
        data,
        {"flowStrength", "force", "strength"},
        0.0,
        -30.0,
        30.0));

    ri::world::FluidSimulationVolume volume = ri::world::CreateFluidSimulationVolume(
        BuildSeedForFluidVolume(data, defaults),
        static_cast<float>(ReadClampedNumber(data, {"gravityScale"}, 0.35, -2.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"jumpScale"}, 0.72, 0.0, 4.0)),
        static_cast<float>(ReadClampedNumber(data, {"drag"}, 1.8, 0.0, 8.0)),
        static_cast<float>(ReadClampedNumber(data, {"buoyancy"}, 0.9, 0.0, 3.0)),
        flowDirection * flowStrength,
        static_cast<float>(ReadClampedNumber(data, {"tintStrength"}, 0.22, 0.0, 1.0)),
        static_cast<float>(ReadClampedNumber(data, {"reverbMix"}, 0.35, 0.0, 1.0)),
        static_cast<float>(ReadClampedNumber(data, {"echoDelayMs"}, 120.0, 0.0, 2000.0)),
        defaults);
    volume.tintColor = ReadColor(data, {"underwaterTint", "tintColor"}, {0.420f, 0.737f, 1.000f});
    return volume;
}

} // namespace ri::content
