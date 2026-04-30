#include "RawIron/Logic/LogicGraph.h"
#include "RawIron/Logic/WorldActorPorts.h"
#include "RawIron/World/RuntimeState.h"
#include "RawIron/World/InteractionPromptState.h"
#include "RawIron/World/VolumeDescriptors.h"
#include "RawIron/World/Instrumentation.h"
#include "RawIron/World/WorldLogicBridge.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>

namespace ri::world {
namespace {

float ClampFloat(float value, float minValue, float maxValue) {
    if (!std::isfinite(value)) {
        return minValue;
    }
    return std::max(minValue, std::min(maxValue, value));
}

float ClampFloatWithFallback(float value, float fallback, float minValue, float maxValue) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::max(minValue, std::min(maxValue, value));
}

void FinalizeLabel(std::vector<std::string>& activeVolumes, std::string& label) {
    if (activeVolumes.empty()) {
        label = "none";
        return;
    }
    label.clear();
    for (std::size_t index = 0; index < activeVolumes.size(); ++index) {
        if (index > 0) {
            label += ',';
        }
        label += activeVolumes[index];
    }
}

ri::math::Vec3 ClampColor(const ri::math::Vec3& color) {
    return ri::math::Vec3{
        ClampFloat(color.x, 0.0f, 1.0f),
        ClampFloat(color.y, 0.0f, 1.0f),
        ClampFloat(color.z, 0.0f, 1.0f),
    };
}

bool ContainsAxis(const std::vector<ConstraintAxis>& values, ConstraintAxis axis) {
    return std::find(values.begin(), values.end(), axis) != values.end();
}

bool CaseInsensitiveEquals(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        const char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(left[index])));
        const char rc = static_cast<char>(std::tolower(static_cast<unsigned char>(right[index])));
        if (lc != rc) {
            return false;
        }
    }
    return true;
}

std::string SanitizeWorldMetadataKey(std::string_view input) {
    std::string sanitized;
    sanitized.reserve(input.size());
    for (const char ch : input) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (std::isalnum(value) != 0) {
            sanitized.push_back(static_cast<char>(std::tolower(value)));
        } else if (!sanitized.empty() && sanitized.back() != '.') {
            sanitized.push_back('.');
        }
    }
    while (!sanitized.empty() && sanitized.back() == '.') {
        sanitized.pop_back();
    }
    return sanitized;
}

bool ContainsVisibilityKind(const VisibilityPrimitive& primitive, VisibilityPrimitiveKind kind) {
    return primitive.kind == kind;
}

float ProjectDistanceOnRay(const ri::math::Vec3& origin, const ri::math::Vec3& normalizedDirection, const ri::math::Vec3& point) {
    return ri::math::Dot(point - origin, normalizedDirection);
}

bool IsPointNearRay(const ri::math::Vec3& origin,
                    const ri::math::Vec3& normalizedDirection,
                    const ri::math::Vec3& point,
                    float maxDistance,
                    float radius) {
    const float projected = ProjectDistanceOnRay(origin, normalizedDirection, point);
    if (projected < 0.0f || projected > maxDistance) {
        return false;
    }
    const ri::math::Vec3 closest = origin + (normalizedDirection * projected);
    return ri::math::DistanceSquared(point, closest) <= (radius * radius);
}

float DistanceToVolumeBounds(const ri::math::Vec3& point, const RuntimeVolume& volume) {
    if (volume.shape == VolumeShape::Box) {
        const float dx = std::max(0.0f, std::fabs(point.x - volume.position.x) - (volume.size.x * 0.5f));
        const float dy = std::max(0.0f, std::fabs(point.y - volume.position.y) - (volume.size.y * 0.5f));
        const float dz = std::max(0.0f, std::fabs(point.z - volume.position.z) - (volume.size.z * 0.5f));
        return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    }
    if (volume.shape == VolumeShape::Cylinder) {
        const float radius = std::max(0.25f, std::isfinite(volume.radius) ? volume.radius : 0.5f);
        const float height = std::max(0.25f, std::isfinite(volume.height) ? volume.height : volume.size.y);
        const float dx = point.x - volume.position.x;
        const float dz = point.z - volume.position.z;
        const float horizontal = std::max(0.0f, std::sqrt((dx * dx) + (dz * dz)) - radius);
        const float vertical = std::max(0.0f, std::fabs(point.y - volume.position.y) - (height * 0.5f));
        return std::sqrt((horizontal * horizontal) + (vertical * vertical));
    }

    const float radius = std::max(0.0f, std::isfinite(volume.radius) ? volume.radius : 0.0f);
    return std::max(0.0f, ri::math::Distance(point, volume.position) - radius);
}

float ComputeBuoyancySubmersionFactor(const ri::math::Vec3& point, const RuntimeVolume& volume) {
    // We model submersion as depth from the local "surface" (top of the volume).
    if (volume.shape == VolumeShape::Sphere) {
        const float radius = std::max(0.25f, std::isfinite(volume.radius) ? volume.radius : 0.5f);
        const float topY = volume.position.y + radius;
        const float normalized = (topY - point.y) / radius;
        return ClampFloat(normalized, 0.0f, 1.0f);
    }

    if (volume.shape == VolumeShape::Cylinder) {
        const float height = std::max(0.25f, std::isfinite(volume.height) ? volume.height : std::max(0.25f, volume.size.y));
        const float topY = volume.position.y + (height * 0.5f);
        const float normalized = (topY - point.y) / (height * 0.5f);
        return ClampFloat(normalized, 0.0f, 1.0f);
    }

    const float boxHeight = std::max(0.25f, std::fabs(volume.size.y));
    const float boxTopY = volume.position.y + (boxHeight * 0.5f);
    const float normalized = (boxTopY - point.y) / (boxHeight * 0.5f);
    return ClampFloat(normalized, 0.0f, 1.0f);
}

std::pair<ri::math::Vec3, ri::math::Vec3> SampleSplinePathFollower(
    const std::vector<ri::math::Vec3>& points,
    bool loop,
    float progress) {
    if (points.empty()) {
        return {ri::math::Vec3{}, ri::math::Vec3{0.0f, 0.0f, 1.0f}};
    }
    if (points.size() == 1U) {
        return {points.front(), ri::math::Vec3{0.0f, 0.0f, 1.0f}};
    }
    const float clampedProgress = ClampFloat(progress, 0.0f, 1.0f);
    const float scaled = clampedProgress * static_cast<float>(points.size() - 1U);
    const std::size_t i0 = static_cast<std::size_t>(std::floor(scaled));
    const std::size_t i1 = std::min(points.size() - 1U, i0 + 1U);
    const float localT = ClampFloat(scaled - static_cast<float>(i0), 0.0f, 1.0f);
    const ri::math::Vec3 a = points[i0];
    const ri::math::Vec3 b = points[i1];
    const ri::math::Vec3 pos = a + ((b - a) * localT);
    ri::math::Vec3 fwd = b - a;
    if (ri::math::LengthSquared(fwd) <= 0.000001f) {
        if (loop && points.size() > 2U) {
            fwd = points[(i1 + 1U) % points.size()] - b;
        }
    }
    if (ri::math::LengthSquared(fwd) <= 0.000001f) {
        fwd = {0.0f, 0.0f, 1.0f};
    } else {
        fwd = ri::math::Normalize(fwd);
    }
    return {pos, fwd};
}

struct SplineClosestPointResult {
    ri::math::Vec3 point{};
    float distance = 0.0f;
    float t = 0.0f;
};

SplineClosestPointResult GetClosestPointOnSplinePath(const ri::math::Vec3& point,
                                                     const std::vector<ri::math::Vec3>& splinePoints) {
    if (splinePoints.empty()) {
        return {};
    }
    if (splinePoints.size() == 1U) {
        return {.point = splinePoints.front(), .distance = ri::math::Distance(splinePoints.front(), point), .t = 0.0f};
    }

    ri::math::Vec3 bestPoint = splinePoints.front();
    float bestDistance = std::numeric_limits<float>::infinity();
    float bestT = 0.0f;
    float totalLength = 0.0f;
    std::vector<float> segmentLengths;
    segmentLengths.reserve(splinePoints.size() - 1U);
    for (std::size_t index = 0; index + 1U < splinePoints.size(); ++index) {
        const float length = ri::math::Distance(splinePoints[index], splinePoints[index + 1U]);
        segmentLengths.push_back(length);
        totalLength += length;
    }

    float traversed = 0.0f;
    for (std::size_t index = 0; index + 1U < splinePoints.size(); ++index) {
        const ri::math::Vec3 start = splinePoints[index];
        const ri::math::Vec3 end = splinePoints[index + 1U];
        const ri::math::Vec3 segment = end - start;
        const float lengthSq = ri::math::Dot(segment, segment);
        const float localT = lengthSq > 0.00000001f
            ? ClampFloat(ri::math::Dot(point - start, segment) / lengthSq, 0.0f, 1.0f)
            : 0.0f;
        const ri::math::Vec3 closest = start + (segment * localT);
        const float distance = ri::math::Distance(closest, point);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestPoint = closest;
            bestT = totalLength > 0.000001f
                ? (traversed + (segmentLengths[index] * localT)) / totalLength
                : 0.0f;
        }
        traversed += segmentLengths[index];
    }

    return {.point = bestPoint, .distance = bestDistance, .t = bestT};
}

ri::math::Vec3 SanitizeFlow(const ri::math::Vec3& flow) {
    return ri::math::Vec3{
        std::isfinite(flow.x) ? flow.x : 0.0f,
        std::isfinite(flow.y) ? flow.y : 0.0f,
        std::isfinite(flow.z) ? flow.z : 0.0f,
    };
}

std::string BuildTriggerIndexKey(std::string_view volumeType, std::string_view volumeId) {
    std::string key;
    key.reserve(volumeType.size() + volumeId.size() + 1U);
    key.append(volumeType);
    key.push_back(':');
    key.append(volumeId);
    return key;
}

std::string FormatMeasureScalar(double value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream << std::setprecision(2) << value;
    return stream.str();
}

int ResolveEffectiveSurfaceResolution(int renderResolution, int resolutionCap) {
    const int sanitizedRenderResolution = std::clamp(renderResolution, 64, 1024);
    const int sanitizedCap = std::clamp(resolutionCap, 64, 2048);
    return std::min(sanitizedRenderResolution, sanitizedCap);
}

bool IsSurfaceUpdateFrame(std::uint64_t frameIndex, std::uint32_t updateEveryFrames) {
    const std::uint32_t cadence = std::clamp<std::uint32_t>(updateEveryFrames, 1U, 120U);
    if (cadence <= 1U) {
        return true;
    }
    return (frameIndex % static_cast<std::uint64_t>(cadence)) == 0ULL;
}

float ComputePassThroughOpacity(const PassThroughPrimitive& primitive,
                                const ri::math::Vec3& viewerPosition,
                                double timeSeconds) {
    float opacity = ClampFloatWithFallback(primitive.material.opacity, 0.35f, 0.0f, 1.0f);

    if (primitive.visualBehavior.pulseEnabled) {
        const float pulseSpeed = ClampFloatWithFallback(primitive.visualBehavior.pulseSpeed, 1.2f, 0.0f, 24.0f);
        const float phase = static_cast<float>(timeSeconds) * pulseSpeed * 6.28318530718f;
        const float t = 0.5f + (0.5f * std::sin(phase));
        const float pulseMin = ClampFloatWithFallback(primitive.visualBehavior.pulseMinOpacity, 0.20f, 0.0f, 1.0f);
        const float pulseMax = ClampFloatWithFallback(primitive.visualBehavior.pulseMaxOpacity, 0.45f, 0.0f, 1.0f);
        const float minOpacity = std::min(pulseMin, pulseMax);
        const float maxOpacity = std::max(pulseMin, pulseMax);
        opacity = minOpacity + ((maxOpacity - minOpacity) * t);
    }

    if (primitive.visualBehavior.distanceFadeEnabled) {
        const float distance = ri::math::Distance(viewerPosition, primitive.position);
        const float fadeNear = std::max(0.0f, primitive.visualBehavior.fadeNear);
        const float fadeFar = std::max(fadeNear + 0.0001f, primitive.visualBehavior.fadeFar);
        const float fadeFactor = ClampFloat(1.0f - ((distance - fadeNear) / (fadeFar - fadeNear)), 0.0f, 1.0f);
        opacity *= fadeFactor;
    }

    return ClampFloat(opacity, 0.0f, 1.0f);
}

ri::math::Vec3 ComputeSkyProjectionParallaxOffset(const SkyProjectionSurface& surface, const ri::math::Vec3& cameraPosition) {
    const float factor = ClampFloatWithFallback(surface.behavior.parallaxFactor, 0.0f, 0.0f, 1.0f);
    if (factor <= 0.0001f) {
        return {};
    }

    const ri::math::Vec3 delta = cameraPosition - surface.position;
    return ri::math::Vec3{
        delta.x * factor,
        surface.behavior.distanceLock ? 0.0f : delta.y * (factor * 0.25f),
        delta.z * factor,
    };
}

float ComputeLodSwitchMetricValue(const LodSwitchPrimitive& primitive,
                                  const ri::math::Vec3& viewerPosition,
                                  float screenSizeMetric) {
    if (primitive.policy.metric == LodSwitchMetric::ScreenSize) {
        return ClampFloatWithFallback(screenSizeMetric, 0.0f, 0.0f, 1000000.0f);
    }
    return ri::math::Distance(viewerPosition, primitive.position);
}

std::size_t SelectLodSwitchLevel(const LodSwitchPrimitive& primitive,
                                 float metricValue) {
    if (primitive.levels.empty()) {
        return 0U;
    }

    const std::size_t currentIndex = std::min(primitive.activeLevelIndex, primitive.levels.size() - 1U);
    if (primitive.policy.hysteresisEnabled) {
        const LodSwitchLevel& current = primitive.levels[currentIndex];
        if (metricValue >= current.distanceEnter && metricValue <= current.distanceExit) {
            return currentIndex;
        }
    }

    for (std::size_t index = 0; index < primitive.levels.size(); ++index) {
        const LodSwitchLevel& level = primitive.levels[index];
        if (metricValue >= level.distanceEnter && metricValue <= level.distanceExit) {
            return index;
        }
    }

    float bestDistance = std::numeric_limits<float>::max();
    std::size_t bestIndex = currentIndex;
    for (std::size_t index = 0; index < primitive.levels.size(); ++index) {
        const LodSwitchLevel& level = primitive.levels[index];
        const float center = (level.distanceEnter + level.distanceExit) * 0.5f;
        const float delta = std::fabs(metricValue - center);
        if (delta < bestDistance) {
            bestDistance = delta;
            bestIndex = index;
        }
    }
    return bestIndex;
}

std::uint64_t HashSurfaceScatterSignature(const SurfaceScatterVolume& volume, std::uint32_t generatedCount) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto appendByte = [&hash](std::uint8_t value) {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= 1099511628211ULL;
    };
    const auto appendString = [&appendByte](std::string_view value) {
        for (const char character : value) {
            appendByte(static_cast<std::uint8_t>(character));
        }
        appendByte(0xFFU);
    };
    const auto appendU32 = [&appendByte](std::uint32_t value) {
        appendByte(static_cast<std::uint8_t>(value & 0xFFU));
        appendByte(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
        appendByte(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
        appendByte(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    };

    appendString(volume.id);
    appendString(volume.sourceRepresentation.payloadId);
    appendU32(static_cast<std::uint32_t>(volume.sourceRepresentation.kind));
    appendU32(volume.distribution.seed);
    appendU32(generatedCount);
    for (const std::string& targetId : volume.targetIds) {
        appendString(targetId);
    }
    return hash;
}

std::uint32_t ComputeSurfaceScatterGeneratedCount(const SurfaceScatterVolume& volume) {
    const float area = std::max(0.01f, std::fabs(volume.size.x * volume.size.z));
    const std::uint32_t densityDrivenCount = volume.density.densityPerSquareMeter > 0.0f
        ? static_cast<std::uint32_t>(std::round(volume.density.densityPerSquareMeter * area))
        : 0U;
    std::uint32_t requestedCount = std::max(volume.density.count, densityDrivenCount);
    requestedCount = std::min(requestedCount, volume.density.maxPoints);

    if (volume.distribution.minSeparation > 0.0001f) {
        constexpr float kPi = 3.14159265358979323846f;
        const float minAreaPerPoint = kPi * volume.distribution.minSeparation * volume.distribution.minSeparation;
        const std::uint32_t separationCap = minAreaPerPoint > 0.0f
            ? static_cast<std::uint32_t>(std::max(1.0f, std::floor(area / minAreaPerPoint)))
            : requestedCount;
        requestedCount = std::min(requestedCount, separationCap);
    }
    return std::max(1U, requestedCount);
}

bool HasValidSplinePoints(const std::vector<ri::math::Vec3>& points) {
    if (points.size() < 2U) {
        return false;
    }
    for (const ri::math::Vec3& point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
            return false;
        }
    }
    return true;
}

std::uint64_t HashSplineTopology(std::string_view id,
                                 const std::vector<ri::math::Vec3>& points,
                                 std::uint32_t seed,
                                 std::uint32_t sampleCount) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto appendByte = [&hash](std::uint8_t value) {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= 1099511628211ULL;
    };
    const auto appendU32 = [&appendByte](std::uint32_t value) {
        appendByte(static_cast<std::uint8_t>(value & 0xFFU));
        appendByte(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
        appendByte(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
        appendByte(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    };
    const auto appendFloat = [&appendU32](float value) {
        const std::uint32_t packed = static_cast<std::uint32_t>(std::round(value * 1000.0f));
        appendU32(packed);
    };

    for (const char character : id) {
        appendByte(static_cast<std::uint8_t>(character));
    }
    appendByte(0xFFU);
    appendU32(seed);
    appendU32(sampleCount);
    for (const ri::math::Vec3& point : points) {
        appendFloat(point.x);
        appendFloat(point.y);
        appendFloat(point.z);
    }
    return hash;
}

bool ProceduralUvTextureSetValid(std::string_view sharedTextureId,
                                 std::string_view textureX,
                                 std::string_view textureY,
                                 std::string_view textureZ) {
    if (!sharedTextureId.empty()) {
        return true;
    }
    return !textureX.empty() && !textureY.empty() && !textureZ.empty();
}

std::uint64_t HashProceduralUvProjectionConfig(
    std::uint8_t kindTag,
    std::string_view id,
    std::string_view remapMode,
    const std::vector<std::string>& targetIds,
    std::string_view textureX,
    std::string_view textureY,
    std::string_view textureZ,
    std::string_view sharedTextureId,
    float projectionScale,
    float blendSharpness,
    const ri::math::Vec3& axisWeights,
    std::uint32_t maxMaterialPatches,
    bool objectSpaceAxes,
    const ProceduralUvProjectionDebugControls& debug) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto appendByte = [&hash](std::uint8_t value) {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= 1099511628211ULL;
    };
    const auto appendString = [&appendByte](std::string_view value) {
        for (const char character : value) {
            appendByte(static_cast<std::uint8_t>(character));
        }
        appendByte(0xFFU);
    };
    const auto appendU32 = [&appendByte](std::uint32_t value) {
        appendByte(static_cast<std::uint8_t>(value & 0xFFU));
        appendByte(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
        appendByte(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
        appendByte(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    };
    const auto appendFloat = [&appendU32](float value) {
        const std::uint32_t packed = static_cast<std::uint32_t>(std::round(value * 1000.0f));
        appendU32(packed);
    };

    appendByte(kindTag);
    appendString(id);
    appendString(remapMode);
    for (const std::string& targetId : targetIds) {
        appendString(targetId);
    }
    appendString(textureX);
    appendString(textureY);
    appendString(textureZ);
    appendString(sharedTextureId);
    appendFloat(projectionScale);
    appendFloat(blendSharpness);
    appendFloat(axisWeights.x);
    appendFloat(axisWeights.y);
    appendFloat(axisWeights.z);
    appendU32(maxMaterialPatches);
    appendByte(objectSpaceAxes ? 1U : 0U);
    appendByte(debug.previewTint ? 1U : 0U);
    appendByte(debug.targetOutlines ? 1U : 0U);
    appendByte(debug.axisContributionPreview ? 1U : 0U);
    appendByte(debug.texelDensityPreview ? 1U : 0U);
    return hash;
}

void EmitTriggerChangedEvent(ri::runtime::RuntimeEventBus* eventBus,
                             const std::string& volumeId,
                             std::string_view volumeType,
                             TriggerTransitionKind kind) {
    if (eventBus == nullptr || kind == TriggerTransitionKind::Stay) {
        return;
    }

    ri::runtime::RuntimeEvent event{};
    event.fields = {
        {"triggerId", volumeId},
        {"state", ToString(kind)},
        {"type", std::string(volumeType)},
    };
    eventBus->Emit("triggerChanged", std::move(event));
}

void EmitGameplayFlowEvent(ri::runtime::RuntimeEventBus* eventBus,
                           std::string_view eventType,
                           const std::string& volumeId,
                           const std::string& value) {
    if (eventBus == nullptr) {
        return;
    }
    ri::runtime::RuntimeEvent event{};
    event.fields = {
        {"volumeId", volumeId},
        {"value", value},
    };
    eventBus->Emit(eventType, std::move(event));
}

void RecordTriggerTransition(TriggerUpdateResult& result,
                             const std::string& volumeId,
                             const std::string& volumeType,
                             TriggerTransitionKind kind,
                             ri::runtime::RuntimeEventBus* eventBus) {
    result.transitions.push_back(TriggerTransition{
        .volumeId = volumeId,
        .volumeType = volumeType,
        .kind = kind,
    });
    EmitTriggerChangedEvent(eventBus, volumeId, volumeType, kind);
}

std::string NormalizeAsciiLower(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c >= 'A' && c <= 'Z') {
            out.push_back(static_cast<char>(c - 'A' + 'a'));
        } else {
            out.push_back(c);
        }
    }
    return out;
}

void EmitLogicWorldActorOutput(ri::logic::LogicGraph& graph,
                               std::string_view actorId,
                               std::string_view outputName,
                               const ri::logic::LogicContext& base) {
    ri::logic::LogicContext ctx = base;
    ctx.sourceId = std::string(actorId);
    graph.EmitWorldOutput(actorId, outputName, std::move(ctx));
}

bool ApplyLogicDoorActorInput(ri::logic::LogicGraph& graph,
                              const std::string& doorId,
                              LogicDoorRuntimeState& door,
                              const std::string& in,
                              const ri::logic::LogicContext& ctx) {
    using namespace ri::logic::ports;
    auto emitOut = [&](std::string_view outName) { EmitLogicWorldActorOutput(graph, doorId, outName, ctx); };

    if (in == "enable" || in == "turnon") {
        door.enabled = true;
        return true;
    }
    if (in == "disable" || in == "turnoff") {
        door.enabled = false;
        return true;
    }
    if (!door.enabled) {
        return true;
    }
    if (in == "open") {
        if (door.locked) {
            return true;
        }
        if (!door.open) {
            door.open = true;
            emitOut(kDoorOnOpened);
        }
        return true;
    }
    if (in == "close") {
        if (door.open) {
            door.open = false;
            emitOut(kDoorOnClosed);
        }
        return true;
    }
    if (in == "lock") {
        if (!door.locked) {
            door.locked = true;
            emitOut(kDoorOnLocked);
        }
        return true;
    }
    if (in == "unlock") {
        door.locked = false;
        return true;
    }
    if (in == "toggle") {
        if (door.locked) {
            return true;
        }
        door.open = !door.open;
        emitOut(door.open ? kDoorOnOpened : kDoorOnClosed);
        return true;
    }
    return true;
}

bool ApplyLogicSpawnerActorInput(ri::logic::LogicGraph& graph,
                                 const std::string& spawnerId,
                                 LogicSpawnerRuntimeState& spawner,
                                 const std::string& in,
                                 const ri::logic::LogicContext& ctx) {
    using namespace ri::logic::ports;
    auto emitOut = [&](std::string_view outName) { EmitLogicWorldActorOutput(graph, spawnerId, outName, ctx); };

    if (in == "enable" || in == "turnon") {
        spawner.enabled = true;
        return true;
    }
    if (in == "disable" || in == "turnoff") {
        spawner.enabled = false;
        return true;
    }
    if (in == "toggle") {
        spawner.enabled = !spawner.enabled;
        return true;
    }
    if (in == "spawn") {
        if (!spawner.enabled) {
            emitOut(kSpawnerOnFailed);
            return true;
        }
        spawner.activeSpawn = true;
        emitOut(kSpawnerOnSpawned);
        return true;
    }
    if (in == "despawn") {
        if (spawner.activeSpawn) {
            spawner.activeSpawn = false;
            emitOut(kSpawnerOnDespawned);
        }
        return true;
    }
    return true;
}

template <typename TVolume, typename TEnterFn, typename TStayFn, typename TExitFn>
void UpdateTriggerFamily(std::vector<TVolume>& volumes,
                         const ri::math::Vec3& position,
                         double elapsedSeconds,
                         const std::unordered_set<std::string>& candidateKeys,
                         TriggerUpdateResult& result,
                         ri::runtime::RuntimeEventBus* eventBus,
                         TEnterFn onEnter,
                         TStayFn onStay,
                         TExitFn onExit) {
    for (TVolume& volume : volumes) {
        const bool shouldEvaluate = volume.playerInside ||
            candidateKeys.contains(BuildTriggerIndexKey(volume.type, volume.id));
        if (!shouldEvaluate) {
            continue;
        }

        if (!volume.armed) {
            if (volume.playerInside) {
                volume.playerInside = false;
                RecordTriggerTransition(result, volume.id, volume.type, TriggerTransitionKind::Exit, eventBus);
                onExit(volume);
            }
            continue;
        }

        const bool inside = IsPointInsideVolume(position, volume);
        if (inside && !volume.playerInside) {
            volume.playerInside = true;
            volume.nextBroadcastAt = 0.0;
            RecordTriggerTransition(result, volume.id, volume.type, TriggerTransitionKind::Enter, eventBus);
            onEnter(volume);
            continue;
        }

        if (inside && volume.playerInside && volume.broadcastFrequency > 0.0 && elapsedSeconds >= volume.nextBroadcastAt) {
            volume.nextBroadcastAt = elapsedSeconds + volume.broadcastFrequency;
            RecordTriggerTransition(result, volume.id, volume.type, TriggerTransitionKind::Stay, eventBus);
            onStay(volume);
            continue;
        }

        if (!inside && volume.playerInside) {
            volume.playerInside = false;
            RecordTriggerTransition(result, volume.id, volume.type, TriggerTransitionKind::Exit, eventBus);
            onExit(volume);
        }
    }
}

} // namespace

ri::spatial::Aabb BuildRuntimeVolumeBounds(const RuntimeVolume& volume) {
    switch (volume.shape) {
    case VolumeShape::Box: {
        const ri::math::Vec3 halfExtents{
            std::max(0.001f, std::fabs(volume.size.x) * 0.5f),
            std::max(0.001f, std::fabs(volume.size.y) * 0.5f),
            std::max(0.001f, std::fabs(volume.size.z) * 0.5f),
        };
        return {
            .min = volume.position - halfExtents,
            .max = volume.position + halfExtents,
        };
    }
    case VolumeShape::Cylinder: {
        const float radius = std::max(0.001f, std::fabs(std::isfinite(volume.radius) ? volume.radius : 0.5f));
        const float halfHeight = std::max(0.001f, std::fabs(std::isfinite(volume.height) ? volume.height : volume.size.y) * 0.5f);
        const ri::math::Vec3 extents{radius, halfHeight, radius};
        return {
            .min = volume.position - extents,
            .max = volume.position + extents,
        };
    }
    case VolumeShape::Sphere:
    default: {
        const float radius = std::max(0.001f, std::fabs(std::isfinite(volume.radius) ? volume.radius : 0.5f));
        const ri::math::Vec3 extents{radius, radius, radius};
        return {
            .min = volume.position - extents,
            .max = volume.position + extents,
        };
    }
    }
}

void RuntimeEnvironmentService::SetPostProcessVolumes(std::vector<PostProcessVolume> volumes) {
    postProcessVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetAudioReverbVolumes(std::vector<AudioReverbVolume> volumes) {
    audioReverbVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetAudioOcclusionVolumes(std::vector<AudioOcclusionVolume> volumes) {
    audioOcclusionVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetAmbientAudioVolumes(std::vector<AmbientAudioVolume> volumes) {
    ambientAudioVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetLocalizedFogVolumes(std::vector<LocalizedFogVolume> volumes) {
    localizedFogVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetFogBlockerVolumes(std::vector<FogBlockerVolume> volumes) {
    fogBlockerVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetFluidSimulationVolumes(std::vector<FluidSimulationVolume> volumes) {
    fluidSimulationVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetPhysicsModifierVolumes(std::vector<PhysicsModifierVolume> volumes) {
    physicsModifierVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetSurfaceVelocityPrimitives(std::vector<SurfaceVelocityPrimitive> volumes) {
    surfaceVelocityPrimitives_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetWaterSurfacePrimitives(std::vector<WaterSurfacePrimitive> primitives) {
    waterSurfacePrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetRadialForceVolumes(std::vector<RadialForceVolume> volumes) {
    radialForceVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetPhysicsConstraintVolumes(std::vector<PhysicsConstraintVolume> volumes) {
    physicsConstraintVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetKinematicTranslationPrimitives(std::vector<KinematicTranslationPrimitive> primitives) {
    kinematicTranslationPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetKinematicRotationPrimitives(std::vector<KinematicRotationPrimitive> primitives) {
    kinematicRotationPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetCameraBlockingVolumes(std::vector<CameraBlockingVolume> volumes) {
    cameraBlockingVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetAiPerceptionBlockerVolumes(std::vector<AiPerceptionBlockerVolume> volumes) {
    aiPerceptionBlockerVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetSafeZoneVolumes(std::vector<SafeZoneRuntimeVolume> volumes) {
    safeZoneVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetTraversalLinkVolumes(std::vector<TraversalLinkVolume> volumes) {
    traversalLinkVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetPivotAnchorPrimitives(std::vector<PivotAnchorPrimitive> primitives) {
    pivotAnchorPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetSymmetryMirrorPlanes(std::vector<SymmetryMirrorPlane> planes) {
    symmetryMirrorPlanes_ = std::move(planes);
}

void RuntimeEnvironmentService::SetLocalGridSnapVolumes(std::vector<LocalGridSnapVolume> volumes) {
    localGridSnapVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetHintPartitionVolumes(std::vector<HintPartitionVolume> volumes) {
    hintPartitionVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetDoorWindowCutoutPrimitives(std::vector<DoorWindowCutoutPrimitive> primitives) {
    doorWindowCutoutPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetProceduralDoorEntities(std::vector<ProceduralDoorEntity> doors) {
    proceduralDoorEntities_ = std::move(doors);
    pendingDoorTransitions_.clear();
    for (const ProceduralDoorEntity& door : proceduralDoorEntities_) {
        logicDoorActors_[door.id] = LogicDoorRuntimeState{
            .open = door.startsOpen,
            .locked = door.startsLocked,
            .enabled = true,
        };
    }
}

void RuntimeEnvironmentService::SetCameraConfinementVolumes(std::vector<CameraConfinementVolume> volumes) {
    cameraConfinementVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetLodOverrideVolumes(std::vector<LodOverrideVolume> volumes) {
    lodOverrideVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetLodSwitchPrimitives(std::vector<LodSwitchPrimitive> primitives) {
    lodSwitchPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetSurfaceScatterVolumes(std::vector<SurfaceScatterVolume> volumes) {
    surfaceScatterVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetSplineMeshDeformerPrimitives(std::vector<SplineMeshDeformerPrimitive> primitives) {
    splineMeshDeformerPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetSplineDecalRibbonPrimitives(std::vector<SplineDecalRibbonPrimitive> primitives) {
    splineDecalRibbonPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetTopologicalUvRemapperVolumes(std::vector<TopologicalUvRemapperVolume> volumes) {
    topologicalUvRemapperVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetTriPlanarNodes(std::vector<TriPlanarNode> nodes) {
    triPlanarNodes_ = std::move(nodes);
}

void RuntimeEnvironmentService::SetInstanceCloudPrimitives(std::vector<InstanceCloudPrimitive> primitives) {
    instanceCloudPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetVoronoiFracturePrimitives(std::vector<VoronoiFracturePrimitive> primitives) {
    voronoiFracturePrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetMetaballPrimitives(std::vector<MetaballPrimitive> primitives) {
    metaballPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetLatticeVolumes(std::vector<LatticeVolume> volumes) {
    latticeVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetManifoldSweepPrimitives(std::vector<ManifoldSweepPrimitive> primitives) {
    manifoldSweepPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetTrimSheetSweepPrimitives(std::vector<TrimSheetSweepPrimitive> primitives) {
    trimSheetSweepPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetLSystemBranchPrimitives(std::vector<LSystemBranchPrimitive> primitives) {
    lSystemBranchPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetGeodesicSpherePrimitives(std::vector<GeodesicSpherePrimitive> primitives) {
    geodesicSpherePrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetExtrudeAlongNormalPrimitives(std::vector<ExtrudeAlongNormalPrimitive> primitives) {
    extrudeAlongNormalPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetSuperellipsoidPrimitives(std::vector<SuperellipsoidPrimitive> primitives) {
    superellipsoidPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetPrimitiveDemoLatticePrimitives(std::vector<PrimitiveDemoLattice> primitives) {
    primitiveDemoLatticePrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetPrimitiveDemoVoronoiPrimitives(std::vector<PrimitiveDemoVoronoi> primitives) {
    primitiveDemoVoronoiPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetThickPolygonPrimitives(std::vector<ThickPolygonPrimitive> primitives) {
    thickPolygonPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetStructuralProfilePrimitives(std::vector<StructuralProfilePrimitive> primitives) {
    structuralProfilePrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetHalfPipePrimitives(std::vector<HalfPipePrimitive> primitives) {
    halfPipePrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetQuarterPipePrimitives(std::vector<QuarterPipePrimitive> primitives) {
    quarterPipePrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetPipeElbowPrimitives(std::vector<PipeElbowPrimitive> primitives) {
    pipeElbowPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetTorusSlicePrimitives(std::vector<TorusSlicePrimitive> primitives) {
    torusSlicePrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetSplineSweepPrimitives(std::vector<SplineSweepPrimitive> primitives) {
    splineSweepPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetRevolvePrimitives(std::vector<RevolvePrimitive> primitives) {
    revolvePrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetDomeVaultPrimitives(std::vector<DomeVaultPrimitive> primitives) {
    domeVaultPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetLoftPrimitives(std::vector<LoftPrimitive> primitives) {
    loftPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetNavmeshModifierVolumes(std::vector<NavmeshModifierVolume> volumes) {
    navmeshModifierVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetReflectionProbeVolumes(std::vector<ReflectionProbeVolume> volumes) {
    reflectionProbeVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetLightImportanceVolumes(std::vector<LightImportanceVolume> volumes) {
    lightImportanceVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetLightPortalVolumes(std::vector<LightPortalVolume> volumes) {
    lightPortalVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetVoxelGiBoundsVolumes(std::vector<VoxelGiBoundsVolume> volumes) {
    voxelGiBoundsVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetLightmapDensityVolumes(std::vector<LightmapDensityVolume> volumes) {
    lightmapDensityVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetShadowExclusionVolumes(std::vector<ShadowExclusionVolume> volumes) {
    shadowExclusionVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetCullingDistanceVolumes(std::vector<CullingDistanceVolume> volumes) {
    cullingDistanceVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetReferenceImagePlanes(std::vector<ReferenceImagePlane> planes) {
    referenceImagePlanes_ = std::move(planes);
}

void RuntimeEnvironmentService::SetText3dPrimitives(std::vector<Text3dPrimitive> textPrimitives) {
    text3dPrimitives_ = std::move(textPrimitives);
}

void RuntimeEnvironmentService::SetAnnotationCommentPrimitives(std::vector<AnnotationCommentPrimitive> comments) {
    annotationCommentPrimitives_ = std::move(comments);
}

void RuntimeEnvironmentService::SetMeasureToolPrimitives(std::vector<MeasureToolPrimitive> tools) {
    measureToolPrimitives_ = std::move(tools);
}

void RuntimeEnvironmentService::SetRenderTargetSurfaces(std::vector<RenderTargetSurface> surfaces) {
    renderTargetSurfaces_ = std::move(surfaces);
}

void RuntimeEnvironmentService::SetPlanarReflectionSurfaces(std::vector<PlanarReflectionSurface> surfaces) {
    planarReflectionSurfaces_ = std::move(surfaces);
}

void RuntimeEnvironmentService::SetPassThroughPrimitives(std::vector<PassThroughPrimitive> primitives) {
    passThroughPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetSkyProjectionSurfaces(std::vector<SkyProjectionSurface> surfaces) {
    skyProjectionSurfaces_ = std::move(surfaces);
}

void RuntimeEnvironmentService::SetDynamicInfoPanelSpawners(std::vector<DynamicInfoPanelSpawner> spawners) {
    dynamicInfoPanelSpawners_ = std::move(spawners);
}

void RuntimeEnvironmentService::SetVolumetricEmitterBounds(std::vector<VolumetricEmitterBounds> volumes) {
    volumetricEmitterBounds_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetSplinePathFollowerPrimitives(std::vector<SplinePathFollowerPrimitive> primitives) {
    splinePathFollowerPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetCablePrimitives(std::vector<CablePrimitive> primitives) {
    cablePrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetClippingVolumes(std::vector<ClippingRuntimeVolume> volumes) {
    clippingVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetFilteredCollisionVolumes(std::vector<FilteredCollisionRuntimeVolume> volumes) {
    filteredCollisionVolumes_ = std::move(volumes);
}

void RuntimeEnvironmentService::SetVisibilityPrimitives(std::vector<VisibilityPrimitive> primitives) {
    visibilityPrimitives_ = std::move(primitives);
}

void RuntimeEnvironmentService::SetOcclusionPortalVolumes(std::vector<OcclusionPortalVolume> volumes) {
    occlusionPortalVolumes_ = std::move(volumes);
    for (const OcclusionPortalVolume& portal : occlusionPortalVolumes_) {
        auto existing = std::find_if(
            visibilityPrimitives_.begin(),
            visibilityPrimitives_.end(),
            [&portal](const VisibilityPrimitive& value) {
                return value.id == portal.id && value.kind == VisibilityPrimitiveKind::OcclusionPortal;
            });
        if (existing == visibilityPrimitives_.end()) {
            VisibilityPrimitive primitive{};
            primitive.id = portal.id;
            primitive.type = "occlusion_portal";
            primitive.debugVisible = portal.debugVisible;
            primitive.kind = VisibilityPrimitiveKind::OcclusionPortal;
            primitive.position = portal.position;
            primitive.size = portal.size;
            primitive.closed = portal.closed;
            visibilityPrimitives_.push_back(std::move(primitive));
        } else {
            existing->position = portal.position;
            existing->size = portal.size;
            existing->closed = portal.closed;
        }
    }
}

void RuntimeEnvironmentService::SetDamageVolumes(std::vector<DamageTriggerVolume> volumes) {
    damageVolumes_ = std::move(volumes);
    MarkTriggerIndexDirty();
}

void RuntimeEnvironmentService::SetGenericTriggerVolumes(std::vector<GenericTriggerVolume> volumes) {
    genericTriggerVolumes_ = std::move(volumes);
    MarkTriggerIndexDirty();
}

void RuntimeEnvironmentService::SetSpatialQueryVolumes(std::vector<SpatialQueryVolume> volumes) {
    spatialQueryVolumes_ = std::move(volumes);
    MarkTriggerIndexDirty();
}

void RuntimeEnvironmentService::SetStreamingLevelVolumes(std::vector<StreamingLevelVolume> volumes) {
    streamingLevelVolumes_ = std::move(volumes);
    MarkTriggerIndexDirty();
}

void RuntimeEnvironmentService::SetCheckpointSpawnVolumes(std::vector<CheckpointSpawnVolume> volumes) {
    checkpointSpawnVolumes_ = std::move(volumes);
    MarkTriggerIndexDirty();
}

void RuntimeEnvironmentService::SetTeleportVolumes(std::vector<TeleportVolume> volumes) {
    teleportVolumes_ = std::move(volumes);
    MarkTriggerIndexDirty();
}

void RuntimeEnvironmentService::SetLaunchVolumes(std::vector<LaunchVolume> volumes) {
    launchVolumes_ = std::move(volumes);
    MarkTriggerIndexDirty();
}

void RuntimeEnvironmentService::SetAnalyticsHeatmapVolumes(std::vector<AnalyticsHeatmapVolume> volumes) {
    analyticsHeatmapVolumes_ = std::move(volumes);
    MarkTriggerIndexDirty();
}

void RuntimeEnvironmentService::SetSpatialQueryTracker(SpatialQueryTracker* tracker) {
    spatialQueryTracker_ = tracker;
}

const std::vector<PostProcessVolume>& RuntimeEnvironmentService::GetPostProcessVolumes() const {
    return postProcessVolumes_;
}

const std::vector<AudioReverbVolume>& RuntimeEnvironmentService::GetAudioReverbVolumes() const {
    return audioReverbVolumes_;
}

const std::vector<AudioOcclusionVolume>& RuntimeEnvironmentService::GetAudioOcclusionVolumes() const {
    return audioOcclusionVolumes_;
}

const std::vector<AmbientAudioVolume>& RuntimeEnvironmentService::GetAmbientAudioVolumes() const {
    return ambientAudioVolumes_;
}

const std::vector<LocalizedFogVolume>& RuntimeEnvironmentService::GetLocalizedFogVolumes() const {
    return localizedFogVolumes_;
}

const std::vector<FogBlockerVolume>& RuntimeEnvironmentService::GetFogBlockerVolumes() const {
    return fogBlockerVolumes_;
}

const std::vector<FluidSimulationVolume>& RuntimeEnvironmentService::GetFluidSimulationVolumes() const {
    return fluidSimulationVolumes_;
}

const std::vector<PhysicsModifierVolume>& RuntimeEnvironmentService::GetPhysicsModifierVolumes() const {
    return physicsModifierVolumes_;
}

const std::vector<SurfaceVelocityPrimitive>& RuntimeEnvironmentService::GetSurfaceVelocityPrimitives() const {
    return surfaceVelocityPrimitives_;
}

const std::vector<WaterSurfacePrimitive>& RuntimeEnvironmentService::GetWaterSurfacePrimitives() const {
    return waterSurfacePrimitives_;
}

const std::vector<RadialForceVolume>& RuntimeEnvironmentService::GetRadialForceVolumes() const {
    return radialForceVolumes_;
}

const std::vector<PhysicsConstraintVolume>& RuntimeEnvironmentService::GetPhysicsConstraintVolumes() const {
    return physicsConstraintVolumes_;
}

const std::vector<KinematicTranslationPrimitive>& RuntimeEnvironmentService::GetKinematicTranslationPrimitives() const {
    return kinematicTranslationPrimitives_;
}

const std::vector<KinematicRotationPrimitive>& RuntimeEnvironmentService::GetKinematicRotationPrimitives() const {
    return kinematicRotationPrimitives_;
}

const std::vector<CameraBlockingVolume>& RuntimeEnvironmentService::GetCameraBlockingVolumes() const {
    return cameraBlockingVolumes_;
}

const std::vector<AiPerceptionBlockerVolume>& RuntimeEnvironmentService::GetAiPerceptionBlockerVolumes() const {
    return aiPerceptionBlockerVolumes_;
}

const std::vector<SafeZoneRuntimeVolume>& RuntimeEnvironmentService::GetSafeZoneVolumes() const {
    return safeZoneVolumes_;
}

const std::vector<TraversalLinkVolume>& RuntimeEnvironmentService::GetTraversalLinkVolumes() const {
    return traversalLinkVolumes_;
}

const std::vector<PivotAnchorPrimitive>& RuntimeEnvironmentService::GetPivotAnchorPrimitives() const {
    return pivotAnchorPrimitives_;
}

const std::vector<SymmetryMirrorPlane>& RuntimeEnvironmentService::GetSymmetryMirrorPlanes() const {
    return symmetryMirrorPlanes_;
}

const std::vector<LocalGridSnapVolume>& RuntimeEnvironmentService::GetLocalGridSnapVolumes() const {
    return localGridSnapVolumes_;
}

const std::vector<HintPartitionVolume>& RuntimeEnvironmentService::GetHintPartitionVolumes() const {
    return hintPartitionVolumes_;
}

const std::vector<DoorWindowCutoutPrimitive>& RuntimeEnvironmentService::GetDoorWindowCutoutPrimitives() const {
    return doorWindowCutoutPrimitives_;
}

const std::vector<ProceduralDoorEntity>& RuntimeEnvironmentService::GetProceduralDoorEntities() const {
    return proceduralDoorEntities_;
}

const std::vector<CameraConfinementVolume>& RuntimeEnvironmentService::GetCameraConfinementVolumes() const {
    return cameraConfinementVolumes_;
}

const std::vector<LodOverrideVolume>& RuntimeEnvironmentService::GetLodOverrideVolumes() const {
    return lodOverrideVolumes_;
}

const std::vector<LodSwitchPrimitive>& RuntimeEnvironmentService::GetLodSwitchPrimitives() const {
    return lodSwitchPrimitives_;
}

const std::vector<SurfaceScatterVolume>& RuntimeEnvironmentService::GetSurfaceScatterVolumes() const {
    return surfaceScatterVolumes_;
}

const std::vector<SplineMeshDeformerPrimitive>& RuntimeEnvironmentService::GetSplineMeshDeformerPrimitives() const {
    return splineMeshDeformerPrimitives_;
}

const std::vector<SplineDecalRibbonPrimitive>& RuntimeEnvironmentService::GetSplineDecalRibbonPrimitives() const {
    return splineDecalRibbonPrimitives_;
}

const std::vector<TopologicalUvRemapperVolume>& RuntimeEnvironmentService::GetTopologicalUvRemapperVolumes() const {
    return topologicalUvRemapperVolumes_;
}

const std::vector<TriPlanarNode>& RuntimeEnvironmentService::GetTriPlanarNodes() const {
    return triPlanarNodes_;
}

const std::vector<InstanceCloudPrimitive>& RuntimeEnvironmentService::GetInstanceCloudPrimitives() const {
    return instanceCloudPrimitives_;
}

const std::vector<VoronoiFracturePrimitive>& RuntimeEnvironmentService::GetVoronoiFracturePrimitives() const {
    return voronoiFracturePrimitives_;
}

const std::vector<MetaballPrimitive>& RuntimeEnvironmentService::GetMetaballPrimitives() const {
    return metaballPrimitives_;
}

const std::vector<LatticeVolume>& RuntimeEnvironmentService::GetLatticeVolumes() const {
    return latticeVolumes_;
}

const std::vector<ManifoldSweepPrimitive>& RuntimeEnvironmentService::GetManifoldSweepPrimitives() const {
    return manifoldSweepPrimitives_;
}

const std::vector<TrimSheetSweepPrimitive>& RuntimeEnvironmentService::GetTrimSheetSweepPrimitives() const {
    return trimSheetSweepPrimitives_;
}

const std::vector<LSystemBranchPrimitive>& RuntimeEnvironmentService::GetLSystemBranchPrimitives() const {
    return lSystemBranchPrimitives_;
}

const std::vector<GeodesicSpherePrimitive>& RuntimeEnvironmentService::GetGeodesicSpherePrimitives() const {
    return geodesicSpherePrimitives_;
}

const std::vector<ExtrudeAlongNormalPrimitive>& RuntimeEnvironmentService::GetExtrudeAlongNormalPrimitives() const {
    return extrudeAlongNormalPrimitives_;
}

const std::vector<SuperellipsoidPrimitive>& RuntimeEnvironmentService::GetSuperellipsoidPrimitives() const {
    return superellipsoidPrimitives_;
}

const std::vector<PrimitiveDemoLattice>& RuntimeEnvironmentService::GetPrimitiveDemoLatticePrimitives() const {
    return primitiveDemoLatticePrimitives_;
}

const std::vector<PrimitiveDemoVoronoi>& RuntimeEnvironmentService::GetPrimitiveDemoVoronoiPrimitives() const {
    return primitiveDemoVoronoiPrimitives_;
}

const std::vector<ThickPolygonPrimitive>& RuntimeEnvironmentService::GetThickPolygonPrimitives() const {
    return thickPolygonPrimitives_;
}

const std::vector<StructuralProfilePrimitive>& RuntimeEnvironmentService::GetStructuralProfilePrimitives() const {
    return structuralProfilePrimitives_;
}

const std::vector<HalfPipePrimitive>& RuntimeEnvironmentService::GetHalfPipePrimitives() const {
    return halfPipePrimitives_;
}

const std::vector<QuarterPipePrimitive>& RuntimeEnvironmentService::GetQuarterPipePrimitives() const {
    return quarterPipePrimitives_;
}

const std::vector<PipeElbowPrimitive>& RuntimeEnvironmentService::GetPipeElbowPrimitives() const {
    return pipeElbowPrimitives_;
}

const std::vector<TorusSlicePrimitive>& RuntimeEnvironmentService::GetTorusSlicePrimitives() const {
    return torusSlicePrimitives_;
}

const std::vector<SplineSweepPrimitive>& RuntimeEnvironmentService::GetSplineSweepPrimitives() const {
    return splineSweepPrimitives_;
}

const std::vector<RevolvePrimitive>& RuntimeEnvironmentService::GetRevolvePrimitives() const {
    return revolvePrimitives_;
}

const std::vector<DomeVaultPrimitive>& RuntimeEnvironmentService::GetDomeVaultPrimitives() const {
    return domeVaultPrimitives_;
}

const std::vector<LoftPrimitive>& RuntimeEnvironmentService::GetLoftPrimitives() const {
    return loftPrimitives_;
}

const std::vector<NavmeshModifierVolume>& RuntimeEnvironmentService::GetNavmeshModifierVolumes() const {
    return navmeshModifierVolumes_;
}

const std::vector<ReflectionProbeVolume>& RuntimeEnvironmentService::GetReflectionProbeVolumes() const {
    return reflectionProbeVolumes_;
}

const std::vector<LightImportanceVolume>& RuntimeEnvironmentService::GetLightImportanceVolumes() const {
    return lightImportanceVolumes_;
}

const std::vector<LightPortalVolume>& RuntimeEnvironmentService::GetLightPortalVolumes() const {
    return lightPortalVolumes_;
}

const std::vector<VoxelGiBoundsVolume>& RuntimeEnvironmentService::GetVoxelGiBoundsVolumes() const {
    return voxelGiBoundsVolumes_;
}

const std::vector<LightmapDensityVolume>& RuntimeEnvironmentService::GetLightmapDensityVolumes() const {
    return lightmapDensityVolumes_;
}

const std::vector<ShadowExclusionVolume>& RuntimeEnvironmentService::GetShadowExclusionVolumes() const {
    return shadowExclusionVolumes_;
}

const std::vector<CullingDistanceVolume>& RuntimeEnvironmentService::GetCullingDistanceVolumes() const {
    return cullingDistanceVolumes_;
}

const std::vector<ReferenceImagePlane>& RuntimeEnvironmentService::GetReferenceImagePlanes() const {
    return referenceImagePlanes_;
}

const std::vector<Text3dPrimitive>& RuntimeEnvironmentService::GetText3dPrimitives() const {
    return text3dPrimitives_;
}

const std::vector<AnnotationCommentPrimitive>& RuntimeEnvironmentService::GetAnnotationCommentPrimitives() const {
    return annotationCommentPrimitives_;
}

const std::vector<MeasureToolPrimitive>& RuntimeEnvironmentService::GetMeasureToolPrimitives() const {
    return measureToolPrimitives_;
}

const std::vector<RenderTargetSurface>& RuntimeEnvironmentService::GetRenderTargetSurfaces() const {
    return renderTargetSurfaces_;
}

const std::vector<PlanarReflectionSurface>& RuntimeEnvironmentService::GetPlanarReflectionSurfaces() const {
    return planarReflectionSurfaces_;
}

const std::vector<PassThroughPrimitive>& RuntimeEnvironmentService::GetPassThroughPrimitives() const {
    return passThroughPrimitives_;
}

const std::vector<SkyProjectionSurface>& RuntimeEnvironmentService::GetSkyProjectionSurfaces() const {
    return skyProjectionSurfaces_;
}

const std::vector<DynamicInfoPanelSpawner>& RuntimeEnvironmentService::GetDynamicInfoPanelSpawners() const {
    return dynamicInfoPanelSpawners_;
}

const std::vector<VolumetricEmitterBounds>& RuntimeEnvironmentService::GetVolumetricEmitterBounds() const {
    return volumetricEmitterBounds_;
}

const std::vector<SplinePathFollowerPrimitive>& RuntimeEnvironmentService::GetSplinePathFollowerPrimitives() const {
    return splinePathFollowerPrimitives_;
}

const std::vector<CablePrimitive>& RuntimeEnvironmentService::GetCablePrimitives() const {
    return cablePrimitives_;
}

const std::vector<ClippingRuntimeVolume>& RuntimeEnvironmentService::GetClippingVolumes() const {
    return clippingVolumes_;
}

const std::vector<FilteredCollisionRuntimeVolume>& RuntimeEnvironmentService::GetFilteredCollisionVolumes() const {
    return filteredCollisionVolumes_;
}

const std::vector<VisibilityPrimitive>& RuntimeEnvironmentService::GetVisibilityPrimitives() const {
    return visibilityPrimitives_;
}

const std::vector<OcclusionPortalVolume>& RuntimeEnvironmentService::GetOcclusionPortalVolumes() const {
    return occlusionPortalVolumes_;
}

const std::vector<DamageTriggerVolume>& RuntimeEnvironmentService::GetDamageVolumes() const {
    return damageVolumes_;
}

const std::vector<GenericTriggerVolume>& RuntimeEnvironmentService::GetGenericTriggerVolumes() const {
    return genericTriggerVolumes_;
}

const std::vector<SpatialQueryVolume>& RuntimeEnvironmentService::GetSpatialQueryVolumes() const {
    return spatialQueryVolumes_;
}

const std::vector<StreamingLevelVolume>& RuntimeEnvironmentService::GetStreamingLevelVolumes() const {
    return streamingLevelVolumes_;
}

const std::vector<CheckpointSpawnVolume>& RuntimeEnvironmentService::GetCheckpointSpawnVolumes() const {
    return checkpointSpawnVolumes_;
}

const std::vector<TeleportVolume>& RuntimeEnvironmentService::GetTeleportVolumes() const {
    return teleportVolumes_;
}

const std::vector<LaunchVolume>& RuntimeEnvironmentService::GetLaunchVolumes() const {
    return launchVolumes_;
}

const std::vector<AnalyticsHeatmapVolume>& RuntimeEnvironmentService::GetAnalyticsHeatmapVolumes() const {
    return analyticsHeatmapVolumes_;
}

std::size_t RuntimeEnvironmentService::CountVisibilityPrimitives(VisibilityPrimitiveKind kind) const {
    return static_cast<std::size_t>(std::count_if(
        visibilityPrimitives_.begin(),
        visibilityPrimitives_.end(),
        [kind](const VisibilityPrimitive& primitive) {
            return ContainsVisibilityKind(primitive, kind);
        }));
}

std::size_t RuntimeEnvironmentService::CountClosedOcclusionPortals() const {
    return static_cast<std::size_t>(std::count_if(
        occlusionPortalVolumes_.begin(),
        occlusionPortalVolumes_.end(),
        [](const OcclusionPortalVolume& volume) {
            return volume.closed;
        }));
}

bool RuntimeEnvironmentService::SetOcclusionPortalClosed(std::string_view portalId, bool closed) {
    auto portal = std::find_if(
        occlusionPortalVolumes_.begin(),
        occlusionPortalVolumes_.end(),
        [portalId](const OcclusionPortalVolume& volume) {
            return volume.id == portalId;
        });
    if (portal == occlusionPortalVolumes_.end()) {
        return false;
    }

    portal->closed = closed;
    for (VisibilityPrimitive& primitive : visibilityPrimitives_) {
        if (primitive.id == portalId && primitive.kind == VisibilityPrimitiveKind::OcclusionPortal) {
            primitive.closed = closed;
        }
    }
    return true;
}

PostProcessState RuntimeEnvironmentService::GetActivePostProcessStateAt(const ri::math::Vec3& position) const {
    PostProcessState state{};
    ri::math::Vec3 tintAccumulator{};
    float tintWeight = 0.0f;

    for (const PostProcessVolume& volume : postProcessVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        state.activeVolumes.push_back(volume.id);
        state.tintStrength = std::max(state.tintStrength, volume.tintStrength);
        state.blurAmount = std::max(state.blurAmount, volume.blurAmount);
        state.noiseAmount += volume.noiseAmount;
        state.scanlineAmount += volume.scanlineAmount;
        state.barrelDistortion += volume.barrelDistortion;
        state.chromaticAberration += volume.chromaticAberration;
        tintAccumulator = tintAccumulator + (volume.tintColor * std::max(0.0f, volume.tintStrength));
        tintWeight += std::max(0.0f, volume.tintStrength);
    }

    if (tintWeight > 0.000001f) {
        state.tintColor = ClampColor(tintAccumulator / tintWeight);
    }

    bool fogBlocked = false;
    for (const FogBlockerVolume& blocker : fogBlockerVolumes_) {
        if (IsPointInsideVolume(position, blocker)) {
            fogBlocked = true;
            break;
        }
    }

    if (!fogBlocked) {
        for (const LocalizedFogVolume& volume : localizedFogVolumes_) {
            if (!IsPointInsideVolume(position, volume)) {
                continue;
            }
            state.activeVolumes.push_back(volume.id);
            state.tintStrength = std::max(state.tintStrength, volume.tintStrength);
            state.blurAmount = std::max(state.blurAmount, volume.blurAmount);
            state.tintColor = ClampColor(volume.tintColor);
        }
    }

    for (const FluidSimulationVolume& volume : fluidSimulationVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        state.activeVolumes.push_back(volume.id);
        state.tintStrength = std::max(state.tintStrength, volume.tintStrength);
        state.blurAmount = std::max(state.blurAmount, 0.0022f);
        state.tintColor = ClampColor(volume.tintColor);
    }

    state.noiseAmount = ClampFloat(state.noiseAmount, 0.0f, 0.24f);
    state.scanlineAmount = ClampFloat(state.scanlineAmount, 0.0f, 0.12f);
    state.barrelDistortion = ClampFloat(state.barrelDistortion, 0.0f, 0.15f);
    state.chromaticAberration = ClampFloat(state.chromaticAberration, 0.0f, 0.03f);
    FinalizeLabel(state.activeVolumes, state.label);
    return state;
}

AudioOcclusionState RuntimeEnvironmentService::GetActiveAudioOcclusionStateAt(const ri::math::Vec3& position) const {
    AudioOcclusionState state{};
    for (const AudioOcclusionVolume& volume : audioOcclusionVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        state.activeVolumes.push_back(volume.id);
        state.dampening = std::max(state.dampening, volume.occlusionStrength);
        state.volumeScale *= std::isfinite(volume.volumeScale) ? volume.volumeScale : 1.0f;
    }
    state.volumeScale = ClampFloat(state.volumeScale, 0.1f, 1.0f);
    return state;
}

AudioEnvironmentState RuntimeEnvironmentService::GetActiveAudioEnvironmentStateAt(const ri::math::Vec3& position) const {
    AudioEnvironmentState state{};

    for (const AudioReverbVolume& volume : audioReverbVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        state.activeVolumes.push_back(volume.id);
        state.reverbMix = std::max(state.reverbMix, volume.reverbMix);
        state.echoDelayMs = std::max(state.echoDelayMs, volume.echoDelayMs);
        state.echoFeedback = std::max(state.echoFeedback, volume.echoFeedback);
        state.dampening = std::max(state.dampening, volume.dampening);
        state.volumeScale *= std::isfinite(volume.volumeScale) ? volume.volumeScale : 1.0f;
        state.playbackRate *= std::isfinite(volume.playbackRate) ? volume.playbackRate : 1.0f;
    }

    if (!state.activeVolumes.empty()) {
        state.volumeScale = ClampFloat(state.volumeScale, 0.2f, 2.0f);
        state.playbackRate = ClampFloat(state.playbackRate, 0.5f, 1.5f);
    }

    const AudioOcclusionState occlusion = GetActiveAudioOcclusionStateAt(position);
    if (!occlusion.activeVolumes.empty()) {
        state.activeVolumes.insert(state.activeVolumes.end(), occlusion.activeVolumes.begin(), occlusion.activeVolumes.end());
        state.dampening = std::max(state.dampening, occlusion.dampening);
        state.volumeScale *= occlusion.volumeScale;
    }

    for (const FluidSimulationVolume& volume : fluidSimulationVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        state.activeVolumes.push_back(volume.id);
        state.reverbMix = std::max(state.reverbMix, volume.reverbMix);
        state.echoDelayMs = std::max(state.echoDelayMs, volume.echoDelayMs);
    }

    FinalizeLabel(state.activeVolumes, state.label);
    return state;
}

std::vector<AmbientAudioContribution> RuntimeEnvironmentService::GetAmbientAudioContributionsAt(const ri::math::Vec3& position) const {
    std::vector<AmbientAudioContribution> contributions;
    contributions.reserve(ambientAudioVolumes_.size());

    for (const AmbientAudioVolume& volume : ambientAudioVolumes_) {
        float distance = 0.0f;
        if (volume.splinePoints.size() >= 2U) {
            distance = GetClosestPointOnSplinePath(position, volume.splinePoints).distance;
        } else if (IsPointInsideVolume(position, volume)) {
            distance = 0.0f;
        } else {
            distance = DistanceToVolumeBounds(position, volume);
        }

        const float falloff = volume.maxDistance > 0.000001f
            ? ClampFloat(1.0f - (distance / volume.maxDistance), 0.0f, 1.0f)
            : 0.0f;
        const float desiredVolume = volume.baseVolume * falloff;
        if (desiredVolume <= 0.000001f) {
            continue;
        }

        contributions.push_back(AmbientAudioContribution{
            .id = volume.id,
            .label = volume.label,
            .audioPath = volume.audioPath,
            .desiredVolume = desiredVolume,
            .distance = distance,
            .normalizedFalloff = falloff,
        });
    }

    std::sort(contributions.begin(),
              contributions.end(),
              [](const AmbientAudioContribution& lhs, const AmbientAudioContribution& rhs) {
                  if (std::fabs(lhs.desiredVolume - rhs.desiredVolume) > 0.0001f) {
                      return lhs.desiredVolume > rhs.desiredVolume;
                  }
                  if (std::fabs(lhs.distance - rhs.distance) > 0.0001f) {
                      return lhs.distance < rhs.distance;
                  }
                  return lhs.id < rhs.id;
              });
    return contributions;
}

AmbientAudioMixState RuntimeEnvironmentService::GetAmbientAudioMixStateAt(const ri::math::Vec3& position) const {
    AmbientAudioMixState state{};
    state.contributions = GetAmbientAudioContributionsAt(position);
    for (const AmbientAudioContribution& contribution : state.contributions) {
        state.combinedDesiredVolume += contribution.desiredVolume;
    }
    state.combinedDesiredVolume = ClampFloat(state.combinedDesiredVolume, 0.0f, 4.0f);
    if (!state.contributions.empty()) {
        const AmbientAudioContribution& top = state.contributions.front();
        state.topDesiredVolume = top.desiredVolume;
        state.topContributionId = top.id.empty() ? std::string("none") : top.id;
        state.topContributionLabel = top.label.empty() ? std::string("none") : top.label;
    }
    return state;
}

PhysicsVolumeModifiers RuntimeEnvironmentService::GetPhysicsVolumeModifiersAt(const ri::math::Vec3& position) const {
    PhysicsVolumeModifiers state{};

    for (const PhysicsModifierVolume& volume : physicsModifierVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        const bool buoyancyType = CaseInsensitiveEquals(volume.type, "buoyancy_volume");
        const float submersion = buoyancyType ? ComputeBuoyancySubmersionFactor(position, volume) : 1.0f;
        state.gravityScale *= std::isfinite(volume.gravityScale) ? volume.gravityScale : 1.0f;
        state.jumpScale *= std::isfinite(volume.jumpScale) ? volume.jumpScale : 1.0f;
        state.drag += (std::isfinite(volume.drag) ? volume.drag : 0.0f) * submersion;
        state.buoyancy = std::max(state.buoyancy, (std::isfinite(volume.buoyancy) ? volume.buoyancy : 0.0f) * submersion);
        state.flow = state.flow + (SanitizeFlow(volume.flow) * submersion);
        state.activeVolumes.push_back(volume.id);
    }

    for (const FluidSimulationVolume& volume : fluidSimulationVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        const float submersion = ComputeBuoyancySubmersionFactor(position, volume);
        state.gravityScale *= std::isfinite(volume.gravityScale) ? volume.gravityScale : 1.0f;
        state.jumpScale *= std::isfinite(volume.jumpScale) ? volume.jumpScale : 1.0f;
        state.drag += (std::isfinite(volume.drag) ? volume.drag : 0.0f) * submersion;
        state.buoyancy = std::max(state.buoyancy, (std::isfinite(volume.buoyancy) ? volume.buoyancy : 0.0f) * submersion);
        state.flow = state.flow + (SanitizeFlow(volume.flow) * submersion);
        state.activeFluids.push_back(volume.id);
    }

    for (const SurfaceVelocityPrimitive& volume : surfaceVelocityPrimitives_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        state.flow = state.flow + SanitizeFlow(volume.flow);
        state.activeSurfaceVelocity.push_back(volume.id);
    }

    for (const RadialForceVolume& volume : radialForceVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        const ri::math::Vec3 delta = position - volume.position;
        const float distance = std::max(0.0001f, ri::math::Length(delta));
        if (distance <= 0.0001f) {
            continue;
        }

        const float radius = std::max(
            0.25f,
            std::isfinite(volume.radius) ? volume.radius : (std::max(volume.size.x, volume.size.z) * 0.5f));
        const float innerRadius = ClampFloatWithFallback(volume.innerRadius, 0.0f, 0.0f, radius - 0.01f);
        const float exponent = ClampFloatWithFallback(volume.falloff, 1.0f, 0.0f, 4.0f);
        const float strength = ClampFloatWithFallback(volume.strength, 4.2f, -40.0f, 40.0f);
        const float distanceRange = std::max(0.01f, radius - innerRadius);
        const float normalizedDistance = ClampFloat((distance - innerRadius) / distanceRange, 0.0f, 1.0f);
        const float radialFalloff = std::pow(std::max(0.0f, 1.0f - normalizedDistance), exponent);
        state.flow = state.flow + (ri::math::Normalize(delta) * (strength * radialFalloff));
        state.activeRadialForces.push_back(volume.id);
    }

    state.drag = ClampFloat(state.drag, 0.0f, 8.0f);
    state.buoyancy = ClampFloat(state.buoyancy, 0.0f, 3.0f);
    state.gravityScale = ClampFloat(state.gravityScale, -2.0f, 4.0f);
    state.jumpScale = ClampFloat(state.jumpScale, 0.0f, 4.0f);
    return state;
}

std::vector<const WaterSurfacePrimitive*> RuntimeEnvironmentService::GetWaterSurfacePrimitivesAt(
    const ri::math::Vec3& position) const {
    struct Candidate {
        const WaterSurfacePrimitive* primitive = nullptr;
        float distance = 0.0f;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(waterSurfacePrimitives_.size());
    for (const WaterSurfacePrimitive& primitive : waterSurfacePrimitives_) {
        if (!IsPointInsideVolume(position, primitive)) {
            continue;
        }
        candidates.push_back(Candidate{
            .primitive = &primitive,
            .distance = DistanceToVolumeBounds(position, primitive),
        });
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (std::fabs(lhs.primitive->position.y - rhs.primitive->position.y) > 0.0001f) {
            return lhs.primitive->position.y > rhs.primitive->position.y;
        }
        return lhs.distance < rhs.distance;
    });
    std::vector<const WaterSurfacePrimitive*> matches;
    matches.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        matches.push_back(candidate.primitive);
    }
    return matches;
}

const WaterSurfacePrimitive* RuntimeEnvironmentService::GetWaterSurfacePrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const WaterSurfacePrimitive*> matches = GetWaterSurfacePrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

WaterSurfaceState RuntimeEnvironmentService::GetWaterSurfaceStateAt(const ri::math::Vec3& position, double timeSeconds) const {
    WaterSurfaceState state{};
    state.surface = GetWaterSurfacePrimitiveAt(position);
    if (state.surface == nullptr) {
        return state;
    }
    state.inside = true;
    state.surfaceY = state.surface->position.y + (state.surface->size.y * 0.5f);
    const double wavePhase = timeSeconds * static_cast<double>(state.surface->waveFrequency) * 6.283185307179586;
    state.waveOffset = static_cast<float>(std::sin(wavePhase)) * state.surface->waveAmplitude;
    state.surfaceY += state.waveOffset;
    return state;
}

PhysicsConstraintState RuntimeEnvironmentService::GetPhysicsConstraintStateAt(const ri::math::Vec3& position) const {
    PhysicsConstraintState state{};

    for (const PhysicsConstraintVolume& volume : physicsConstraintVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        for (ConstraintAxis axis : volume.lockAxes) {
            if (!ContainsAxis(state.lockAxes, axis)) {
                state.lockAxes.push_back(axis);
            }
        }
    }

    const auto axisRank = [](ConstraintAxis axis) {
        switch (axis) {
        case ConstraintAxis::X:
            return 0;
        case ConstraintAxis::Y:
            return 1;
        case ConstraintAxis::Z:
            return 2;
        }
        return 3;
    };
    std::sort(state.lockAxes.begin(), state.lockAxes.end(), [axisRank](ConstraintAxis lhs, ConstraintAxis rhs) {
        return axisRank(lhs) < axisRank(rhs);
    });

    return state;
}

std::vector<const KinematicTranslationPrimitive*> RuntimeEnvironmentService::GetKinematicTranslationPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const KinematicTranslationPrimitive*> matches;
    matches.reserve(kinematicTranslationPrimitives_.size());
    for (const KinematicTranslationPrimitive& primitive : kinematicTranslationPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

std::vector<const KinematicRotationPrimitive*> RuntimeEnvironmentService::GetKinematicRotationPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const KinematicRotationPrimitive*> matches;
    matches.reserve(kinematicRotationPrimitives_.size());
    for (const KinematicRotationPrimitive& primitive : kinematicRotationPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

KinematicMotionState RuntimeEnvironmentService::ResolveKinematicMotionAt(const ri::math::Vec3& position, double timeSeconds) const {
    KinematicMotionState state{};
    for (const KinematicTranslationPrimitive* primitive : GetKinematicTranslationPrimitivesAt(position)) {
        if (primitive == nullptr) {
            continue;
        }
        const double cycle = std::max(0.1, static_cast<double>(primitive->cycleSeconds));
        const double period = primitive->pingPong ? cycle * 2.0 : cycle;
        const double phase = std::fmod(std::max(0.0, timeSeconds), period) / cycle;
        double normalized = primitive->pingPong ? (phase <= 1.0 ? phase : (2.0 - phase)) : phase;
        normalized = std::clamp(normalized, 0.0, 1.0);
        state.translationDelta = state.translationDelta + (primitive->axis * (primitive->distance * static_cast<float>(normalized)));
        state.activeTranslationPrimitives.push_back(primitive->id);
    }

    for (const KinematicRotationPrimitive* primitive : GetKinematicRotationPrimitivesAt(position)) {
        if (primitive == nullptr) {
            continue;
        }
        const float speed = primitive->angularSpeedDegreesPerSecond;
        float angle = speed * static_cast<float>(timeSeconds);
        if (primitive->pingPong) {
            const float maxAngle = std::max(0.0f, primitive->maxAngleDegrees);
            if (maxAngle > 0.0001f) {
                const float span = maxAngle * 2.0f;
                const float wrapped = std::fmod(std::fabs(angle), span);
                angle = wrapped <= maxAngle ? wrapped : (span - wrapped);
            }
        } else {
            angle = std::clamp(angle, -primitive->maxAngleDegrees, primitive->maxAngleDegrees);
        }
        state.rotationDeltaDegrees = state.rotationDeltaDegrees + (primitive->axis * angle);
        state.activeRotationPrimitives.push_back(primitive->id);
    }
    return state;
}

bool RuntimeEnvironmentService::IsCameraBlockedAt(const ri::math::Vec3& position, std::string_view traceTag) const {
    return std::any_of(cameraBlockingVolumes_.begin(), cameraBlockingVolumes_.end(), [&](const CameraBlockingVolume& volume) {
        if (!IsPointInsideVolume(position, volume)) {
            return false;
        }
        if (volume.channels.empty()) {
            return true;
        }
        return std::any_of(volume.channels.begin(), volume.channels.end(), [&](const std::string& channel) {
            return CaseInsensitiveEquals(channel, traceTag);
        });
    });
}

AiPerceptionBlockerState RuntimeEnvironmentService::GetAiPerceptionBlockerStateAt(const ri::math::Vec3& position) const {
    AiPerceptionBlockerState state{};
    for (const AiPerceptionBlockerVolume& volume : aiPerceptionBlockerVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        state.matches.push_back(&volume);
        if (volume.enabled) {
            state.anyEnabled = true;
            if (volume.modes.empty() ||
                std::any_of(volume.modes.begin(), volume.modes.end(), [](const std::string& mode) {
                    return CaseInsensitiveEquals(mode, "ai");
                })) {
                state.blocked = true;
            }
        }
    }
    return state;
}

SafeZoneState RuntimeEnvironmentService::GetSafeZoneStateAt(const ri::math::Vec3& position) const {
    SafeZoneState state{};
    for (const SafeZoneRuntimeVolume& volume : safeZoneVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        state.matches.push_back(&volume);
        state.inside = true;
        state.dropAggro = state.dropAggro || volume.dropAggro;
    }
    return state;
}

TraversalLinkSelectionState RuntimeEnvironmentService::GetTraversalLinksAt(const ri::math::Vec3& position) const {
    TraversalLinkSelectionState state{};
    state.matches.reserve(traversalLinkVolumes_.size());
    for (const TraversalLinkVolume& volume : traversalLinkVolumes_) {
        if (IsPointInsideVolume(position, volume)) {
            state.matches.push_back(&volume);
        }
    }
    if (state.matches.empty()) {
        return state;
    }

    const auto linkRank = [](TraversalLinkKind kind) {
        switch (kind) {
        case TraversalLinkKind::Ladder:
            return 3;
        case TraversalLinkKind::Climb:
            return 2;
        case TraversalLinkKind::General:
            return 1;
        }
        return 0;
    };

    state.selected = *std::max_element(
        state.matches.begin(),
        state.matches.end(),
        [linkRank](const TraversalLinkVolume* lhs, const TraversalLinkVolume* rhs) {
            const int lhsRank = linkRank(lhs->kind);
            const int rhsRank = linkRank(rhs->kind);
            if (lhsRank != rhsRank) {
                return lhsRank < rhsRank;
            }
            if (std::fabs(lhs->climbSpeed - rhs->climbSpeed) > 0.0001f) {
                return lhs->climbSpeed < rhs->climbSpeed;
            }
            return lhs->id > rhs->id;
        });
    return state;
}

const TraversalLinkVolume* RuntimeEnvironmentService::GetTraversalLinkAt(const ri::math::Vec3& position) const {
    return GetTraversalLinksAt(position).selected;
}

const PivotAnchorPrimitive* RuntimeEnvironmentService::GetPivotAnchorAt(const ri::math::Vec3& position) const {
    const PivotAnchorPrimitive* best = nullptr;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (const PivotAnchorPrimitive& primitive : pivotAnchorPrimitives_) {
        if (!IsPointInsideVolume(position, primitive)) {
            continue;
        }
        const float distance = ri::math::DistanceSquared(position, primitive.position);
        if (best == nullptr || distance < bestDistance) {
            best = &primitive;
            bestDistance = distance;
        }
    }
    return best;
}

PivotAnchorBindingState RuntimeEnvironmentService::ResolvePivotAnchorBindingAt(
    const ri::math::Vec3& position,
    const ri::math::Vec3& fallbackForward) const {
    PivotAnchorBindingState state{};
    state.resolvedPosition = position;
    state.resolvedForwardAxis = ri::math::LengthSquared(fallbackForward) <= 0.000001f
        ? ri::math::Vec3{0.0f, 0.0f, 1.0f}
        : ri::math::Normalize(fallbackForward);
    state.anchor = GetPivotAnchorAt(position);
    if (state.anchor == nullptr) {
        return state;
    }
    state.resolvedPosition = state.anchor->position;
    state.resolvedForwardAxis = ri::math::LengthSquared(state.anchor->forwardAxis) <= 0.000001f
        ? state.resolvedForwardAxis
        : ri::math::Normalize(state.anchor->forwardAxis);
    return state;
}

const SymmetryMirrorPlane* RuntimeEnvironmentService::GetSymmetryMirrorPlaneAt(const ri::math::Vec3& position) const {
    const SymmetryMirrorPlane* best = nullptr;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (const SymmetryMirrorPlane& plane : symmetryMirrorPlanes_) {
        if (!IsPointInsideVolume(position, plane)) {
            continue;
        }
        const float signedDistance = ri::math::Dot(plane.planeNormal, position - plane.position) - plane.planeOffset;
        const float absDistance = std::fabs(signedDistance);
        if (best == nullptr || absDistance < bestDistance) {
            best = &plane;
            bestDistance = absDistance;
        }
    }
    return best;
}

SymmetryMirrorResult RuntimeEnvironmentService::ResolveSymmetryMirrorAt(const ri::math::Vec3& position,
                                                                        const ri::math::Vec3& forwardDirection) const {
    SymmetryMirrorResult result{};
    result.mirroredPosition = position;
    result.mirroredForward = ri::math::LengthSquared(forwardDirection) <= 0.000001f
        ? ri::math::Vec3{0.0f, 0.0f, 1.0f}
        : ri::math::Normalize(forwardDirection);
    result.plane = GetSymmetryMirrorPlaneAt(position);
    if (result.plane == nullptr) {
        return result;
    }

    result.signedDistanceToPlane = ri::math::Dot(result.plane->planeNormal, position - result.plane->position)
        - result.plane->planeOffset;
    result.mirroredPosition = position - (result.plane->planeNormal * (2.0f * result.signedDistanceToPlane));

    const float directionDot = ri::math::Dot(result.plane->planeNormal, result.mirroredForward);
    result.mirroredForward = result.mirroredForward - (result.plane->planeNormal * (2.0f * directionDot));
    if (ri::math::LengthSquared(result.mirroredForward) <= 0.000001f) {
        result.mirroredForward = {0.0f, 0.0f, 1.0f};
    } else {
        result.mirroredForward = ri::math::Normalize(result.mirroredForward);
    }
    result.mirrored = true;
    return result;
}

AuthoringPlacementState RuntimeEnvironmentService::ResolveAuthoringPlacementAt(const ri::math::Vec3& position,
                                                                               const ri::math::Vec3& forwardDirection) const {
    AuthoringPlacementState state{};
    const PivotAnchorBindingState pivot = ResolvePivotAnchorBindingAt(position, forwardDirection);
    state.pivotAnchor = pivot.anchor;
    state.resolvedPosition = pivot.resolvedPosition;
    state.resolvedForward = pivot.resolvedForwardAxis;

    const SymmetryMirrorResult mirror = ResolveSymmetryMirrorAt(state.resolvedPosition, state.resolvedForward);
    state.mirrorPlane = mirror.plane;
    if (mirror.mirrored) {
        state.resolvedPosition = mirror.mirroredPosition;
        state.resolvedForward = mirror.mirroredForward;
        state.mirrored = true;
    }

    if (state.mirrorPlane != nullptr && state.mirrorPlane->snapToGrid) {
        state.resolvedPosition = SnapPositionToLocalGrid(state.resolvedPosition);
        state.snappedToGrid = true;
    }
    return state;
}

const LocalGridSnapVolume* RuntimeEnvironmentService::GetLocalGridSnapAt(const ri::math::Vec3& position) const {
    const LocalGridSnapVolume* best = nullptr;
    float bestSnapSize = 16.0f;
    float bestDistanceToCenter = std::numeric_limits<float>::infinity();

    for (const LocalGridSnapVolume& volume : localGridSnapVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }

        const float snapSize = ClampFloatWithFallback(volume.snapSize, 0.5f, 0.01f, 16.0f);
        const float distanceToCenter = ri::math::Distance(position, volume.position);
        if (best == nullptr ||
            volume.priority > best->priority ||
            (volume.priority == best->priority && snapSize < bestSnapSize) ||
            (volume.priority == best->priority && std::fabs(snapSize - bestSnapSize) <= 0.0001f &&
             distanceToCenter < bestDistanceToCenter)) {
            best = &volume;
            bestSnapSize = snapSize;
            bestDistanceToCenter = distanceToCenter;
        }
    }

    return best;
}

ri::math::Vec3 RuntimeEnvironmentService::SnapPositionToLocalGrid(const ri::math::Vec3& position) const {
    const LocalGridSnapVolume* grid = GetLocalGridSnapAt(position);
    if (grid == nullptr) {
        return position;
    }
    const float snapSize = ClampFloatWithFallback(grid->snapSize, 0.5f, 0.01f, 16.0f);
    const ri::math::Vec3 delta = position - grid->position;
    const auto snapAxis = [snapSize](float value) {
        return std::round(value / snapSize) * snapSize;
    };
    ri::math::Vec3 snapped = position;
    if (grid->snapX) {
        snapped.x = grid->position.x + snapAxis(delta.x);
    }
    if (grid->snapY) {
        snapped.y = grid->position.y + snapAxis(delta.y);
    }
    if (grid->snapZ) {
        snapped.z = grid->position.z + snapAxis(delta.z);
    }
    return snapped;
}

const HintPartitionVolume* RuntimeEnvironmentService::GetHintPartitionVolumeAt(const ri::math::Vec3& position) const {
    auto it = std::find_if(
        hintPartitionVolumes_.begin(),
        hintPartitionVolumes_.end(),
        [&position](const HintPartitionVolume& volume) {
            return IsPointInsideVolume(position, volume);
        });
    return it == hintPartitionVolumes_.end() ? nullptr : &(*it);
}

HintPartitionState RuntimeEnvironmentService::GetHintPartitionStateAt(const ri::math::Vec3& position) const {
    HintPartitionState state{};
    state.volume = GetHintPartitionVolumeAt(position);
    state.inside = state.volume != nullptr;
    if (state.volume != nullptr) {
        state.mode = state.volume->mode;
    }
    return state;
}

const DoorWindowCutoutPrimitive* RuntimeEnvironmentService::GetDoorWindowCutoutAt(const ri::math::Vec3& position) const {
    const DoorWindowCutoutPrimitive* best = nullptr;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (const DoorWindowCutoutPrimitive& primitive : doorWindowCutoutPrimitives_) {
        if (!IsPointInsideVolume(position, primitive)) {
            continue;
        }
        const float distance = ri::math::DistanceSquared(position, primitive.position);
        if (best == nullptr || distance < bestDistance) {
            best = &primitive;
            bestDistance = distance;
        }
    }
    return best;
}

const ProceduralDoorEntity* RuntimeEnvironmentService::GetProceduralDoorEntityAt(const ri::math::Vec3& position) const {
    const ProceduralDoorEntity* best = nullptr;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (const ProceduralDoorEntity& door : proceduralDoorEntities_) {
        if (!IsPointInsideVolume(position, door)) {
            continue;
        }
        const float distance = ri::math::DistanceSquared(position, door.position);
        if (best == nullptr || distance < bestDistance) {
            best = &door;
            bestDistance = distance;
        }
    }
    return best;
}

const DynamicInfoPanelSpawner* RuntimeEnvironmentService::GetDynamicInfoPanelSpawnerAt(const ri::math::Vec3& position) const {
    const DynamicInfoPanelSpawner* best = nullptr;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (const DynamicInfoPanelSpawner& panel : dynamicInfoPanelSpawners_) {
        RuntimeVolume probe{};
        probe.position = panel.position;
        probe.size = panel.size;
        probe.shape = VolumeShape::Box;
        if (!IsPointInsideVolume(position, probe)) {
            continue;
        }
        const float distance = ri::math::DistanceSquared(position, panel.position);
        if (best == nullptr || distance < bestDistance) {
            best = &panel;
            bestDistance = distance;
        }
    }
    return best;
}

std::string RuntimeEnvironmentService::GetInfoPanelInteractionPromptAt(const ri::math::Vec3& position) const {
    const DynamicInfoPanelSpawner* panel = GetDynamicInfoPanelSpawnerAt(position);
    if (panel == nullptr) {
        return {};
    }
    return panel->interactionPrompt.empty() ? std::string("Read Panel") : panel->interactionPrompt;
}

InteractionTargetState RuntimeEnvironmentService::ResolveInteractionTarget(const ri::math::Vec3& origin,
                                                                           const ri::math::Vec3& forward,
                                                                           const InteractionTargetOptions& options) const {
    InteractionTargetState best{};
    const float maxDistance = ClampFloat(options.maxDistance, 0.5f, 12.0f);
    const float overlapRadius = ClampFloat(options.overlapRadius, 0.1f, 4.0f);
    ri::math::Vec3 rayDirection = forward;
    if (ri::math::LengthSquared(rayDirection) <= 0.00001f) {
        rayDirection = {0.0f, 0.0f, 1.0f};
    } else {
        rayDirection = ri::math::Normalize(rayDirection);
    }

    const auto kindRank = [](InteractionTargetKind kind) {
        switch (kind) {
        case InteractionTargetKind::Door:
            return 0;
        case InteractionTargetKind::InfoPanel:
            return 1;
        case InteractionTargetKind::None:
        default:
            return 2;
        }
    };
    const auto betterCandidate = [&](InteractionTargetKind kind,
                                     std::string_view id,
                                     float directDistance,
                                     bool inRay,
                                     bool inOverlap) {
        if (best.kind == InteractionTargetKind::None) {
            return true;
        }

        // Deterministic overlap conflict resolution: prefer ray-visible targets, then overlaps, then nearest.
        if (inRay != best.inRay) {
            return inRay;
        }
        if (inOverlap != best.inOverlap) {
            return inOverlap;
        }
        if (directDistance + 0.0001f < best.distance) {
            return true;
        }
        if (best.distance + 0.0001f < directDistance) {
            return false;
        }

        const int lhsRank = kindRank(kind);
        const int rhsRank = kindRank(best.kind);
        if (lhsRank != rhsRank) {
            return lhsRank < rhsRank;
        }
        return std::string_view(best.targetId) > id;
    };

    auto consider = [&](InteractionTargetKind kind,
                        std::string_view id,
                        std::string_view interactionHook,
                        std::string_view verb,
                        std::string_view targetLabel,
                        const ri::math::Vec3& position,
                        float selectionRadius) {
        const float directDistance = ri::math::Distance(origin, position);
        const bool inRay = IsPointNearRay(origin, rayDirection, position, maxDistance, selectionRadius);
        const bool inOverlap = directDistance <= overlapRadius;
        if (!inRay && !inOverlap) {
            return;
        }

        if (!betterCandidate(kind, id, directDistance, inRay, inOverlap)) {
            return;
        }

        InteractionPromptState prompt;
        prompt.Show({
            .actionLabel = options.actionLabel,
            .verb = std::string(verb),
            .targetLabel = std::string(targetLabel),
            .fallbackLabel = {},
        });
        const InteractionPromptView promptView = prompt.BuildView();

        best.kind = kind;
        best.targetId = std::string(id);
        best.interactionHook = std::string(interactionHook);
        best.promptText = promptView.text;
        best.distance = directDistance;
        best.inRay = inRay;
        best.inOverlap = inOverlap;
    };

    for (const ProceduralDoorEntity& door : proceduralDoorEntities_) {
        const float doorRadius = std::max(0.2f, std::max(door.size.x, door.size.z) * 0.6f);
        const std::string_view label = door.interactionPrompt.empty() ? std::string_view("Door") : std::string_view(door.interactionPrompt);
        consider(InteractionTargetKind::Door, door.id, door.interactionHook, "Use", label, door.position, doorRadius);
    }

    for (const DynamicInfoPanelSpawner& panel : dynamicInfoPanelSpawners_) {
        const float panelRadius = std::max(0.2f, std::max(panel.size.x, panel.size.z) * 0.6f);
        const std::string_view label = panel.interactionPrompt.empty() ? std::string_view("Panel") : std::string_view(panel.interactionPrompt);
        consider(InteractionTargetKind::InfoPanel, panel.id, panel.interactionHook, "Read", label, panel.position, panelRadius);
    }

    return best;
}

AudioRoutingState RuntimeEnvironmentService::GetAudioRoutingStateAt(const ri::math::Vec3& position) const {
    AudioRoutingState routing{};
    const AudioEnvironmentState environment = GetActiveAudioEnvironmentStateAt(position);
    const AmbientAudioMixState ambient = GetAmbientAudioMixStateAt(position);
    routing.environmentLabel = environment.label;
    routing.ambientLayer = ClampFloat(ambient.combinedDesiredVolume, 0.0f, 1.0f);
    routing.chaseLayer = HasWorldFlag("audio.chase") ? 1.0f : 0.0f;
    routing.endingLayer = HasWorldFlag("audio.ending") ? 1.0f : 0.0f;
    return routing;
}

bool RuntimeEnvironmentService::TryInteractWithProceduralDoor(std::string_view doorId,
                                                              bool hasAccess,
                                                              std::string* outFeedback) {
    const auto doorIt = std::find_if(
        proceduralDoorEntities_.begin(),
        proceduralDoorEntities_.end(),
        [doorId](const ProceduralDoorEntity& value) { return value.id == doorId; });
    if (doorIt == proceduralDoorEntities_.end()) {
        return false;
    }

    auto stateIt = logicDoorActors_.find(doorIt->id);
    if (stateIt == logicDoorActors_.end()) {
        stateIt = logicDoorActors_.emplace(
            doorIt->id,
            LogicDoorRuntimeState{
                .open = doorIt->startsOpen,
                .locked = doorIt->startsLocked,
                .enabled = true,
            }).first;
    }
    LogicDoorRuntimeState& state = stateIt->second;
    if (state.locked && !hasAccess) {
        if (outFeedback != nullptr) {
            *outFeedback = doorIt->deniedPrompt.empty() ? std::string("Locked") : doorIt->deniedPrompt;
        }
        if (!doorIt->accessFeedbackTag.empty()) {
            pendingDoorTransitions_.push_back(DoorTransitionRequest{
                .doorId = doorIt->id,
                .transitionLevel = {},
                .endingTrigger = {},
                .accessFeedbackTag = doorIt->accessFeedbackTag,
            });
        }
        return true;
    }

    if (state.locked && hasAccess) {
        state.locked = false;
    }
    state.open = true;
    if (outFeedback != nullptr) {
        *outFeedback = doorIt->interactionPrompt.empty() ? std::string("Opened") : doorIt->interactionPrompt;
    }

    if (!doorIt->transitionLevel.empty() || !doorIt->endingTrigger.empty() || !doorIt->accessFeedbackTag.empty()) {
        pendingDoorTransitions_.push_back(DoorTransitionRequest{
            .doorId = doorIt->id,
            .transitionLevel = doorIt->transitionLevel,
            .endingTrigger = doorIt->endingTrigger,
            .accessFeedbackTag = doorIt->accessFeedbackTag,
        });
    }
    return true;
}

std::vector<DoorTransitionRequest> RuntimeEnvironmentService::ConsumePendingDoorTransitions() {
    std::vector<DoorTransitionRequest> out = std::move(pendingDoorTransitions_);
    pendingDoorTransitions_.clear();
    return out;
}

void RuntimeEnvironmentService::ApplyDoorTransitionMetadata(const DoorTransitionRequest& request) {
    if (!request.accessFeedbackTag.empty()) {
        const std::string tagKey = SanitizeWorldMetadataKey(request.accessFeedbackTag);
        if (!tagKey.empty()) {
            SetWorldFlag("access_feedback." + tagKey, true);
        }
    }
    if (!request.endingTrigger.empty()) {
        const std::string endingKey = SanitizeWorldMetadataKey(request.endingTrigger);
        if (!endingKey.empty()) {
            SetWorldFlag("ending." + endingKey, true);
        }
    }
    if (!request.transitionLevel.empty()) {
        const std::string levelKey = SanitizeWorldMetadataKey(request.transitionLevel);
        if (!levelKey.empty()) {
            SetWorldFlag("level_transition." + levelKey, true);
        }
    }
    const double transitionCount = GetWorldValueOr("door.transition.count", 0.0);
    SetWorldValue("door.transition.count", transitionCount + 1.0);
}

const CameraConfinementVolume* RuntimeEnvironmentService::GetCameraConfinementVolumeAt(const ri::math::Vec3& position) const {
    auto it = std::find_if(
        cameraConfinementVolumes_.begin(),
        cameraConfinementVolumes_.end(),
        [&position](const CameraConfinementVolume& volume) {
            return IsPointInsideVolume(position, volume);
        });
    return it == cameraConfinementVolumes_.end() ? nullptr : &(*it);
}

std::vector<const LodOverrideVolume*> RuntimeEnvironmentService::GetLodOverridesAt(const ri::math::Vec3& position) const {
    std::vector<const LodOverrideVolume*> matches;
    for (const LodOverrideVolume& volume : lodOverrideVolumes_) {
        if (IsPointInsideVolume(position, volume)) {
            matches.push_back(&volume);
        }
    }
    return matches;
}

LodOverrideSelectionState RuntimeEnvironmentService::ResolveLodOverrideAt(const ri::math::Vec3& position,
                                                                          std::string_view targetId) const {
    LodOverrideSelectionState state{};
    state.matches = GetLodOverridesAt(position);
    if (state.matches.empty()) {
        return state;
    }

    const bool hasTarget = !targetId.empty();
    int bestSpecificity = -1;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (const LodOverrideVolume* match : state.matches) {
        int specificity = 0;
        if (hasTarget) {
            if (match->targetIds.empty()) {
                specificity = 1;
            } else if (std::find(match->targetIds.begin(), match->targetIds.end(), targetId) != match->targetIds.end()) {
                specificity = 2;
            } else {
                continue;
            }
        }

        const float distance = DistanceToVolumeBounds(position, *match);
        if (state.selected == nullptr
            || specificity > bestSpecificity
            || (specificity == bestSpecificity && distance < bestDistance - 0.0001f)
            || (specificity == bestSpecificity && std::fabs(distance - bestDistance) <= 0.0001f
                && match->id < state.selected->id)) {
            state.selected = match;
            bestSpecificity = specificity;
            bestDistance = distance;
        }
    }

    if (state.selected == nullptr) {
        state.selected = state.matches.front();
    }
    state.hasTargetMatch = hasTarget && bestSpecificity >= 2;
    state.forcedLod = state.selected->forcedLod;
    return state;
}

void RuntimeEnvironmentService::UpdateLodSwitchPrimitives(
    const ri::math::Vec3& viewerPosition,
    double timeSeconds,
    float screenSizeMetric) {
    for (LodSwitchPrimitive& primitive : lodSwitchPrimitives_) {
        if (primitive.levels.empty()) {
            continue;
        }

        const float metricValue = ComputeLodSwitchMetricValue(primitive, viewerPosition, screenSizeMetric);
        const std::size_t selectedIndex = SelectLodSwitchLevel(primitive, metricValue);
        if (selectedIndex != primitive.activeLevelIndex) {
            if (primitive.lastSwitchTimeSeconds >= 0.0 && (timeSeconds - primitive.lastSwitchTimeSeconds) < 0.2) {
                primitive.thrashWarnings += 1U;
            }
            primitive.previousLevelIndex = primitive.activeLevelIndex;
            primitive.activeLevelIndex = selectedIndex;
            primitive.switchCount += 1U;
            primitive.lastSwitchTimeSeconds = timeSeconds;
            primitive.crossfadeAlpha = primitive.policy.transitionMode == LodSwitchTransitionMode::Crossfade ? 0.0f : 1.0f;
        }

        if (primitive.policy.transitionMode == LodSwitchTransitionMode::Crossfade
            && primitive.policy.crossfadeSeconds > 0.0001f
            && primitive.crossfadeAlpha < 1.0f
            && primitive.lastSwitchTimeSeconds >= 0.0) {
            const float elapsed = static_cast<float>(std::max(0.0, timeSeconds - primitive.lastSwitchTimeSeconds));
            primitive.crossfadeAlpha = ClampFloat(elapsed / primitive.policy.crossfadeSeconds, 0.0f, 1.0f);
        } else {
            primitive.crossfadeAlpha = 1.0f;
        }
    }
}

std::vector<LodSwitchSelectionState> RuntimeEnvironmentService::GetLodSwitchSelectionStates() const {
    std::vector<LodSwitchSelectionState> states;
    states.reserve(lodSwitchPrimitives_.size());
    for (const LodSwitchPrimitive& primitive : lodSwitchPrimitives_) {
        if (primitive.levels.empty()) {
            continue;
        }

        const std::size_t activeIndex = std::min(primitive.activeLevelIndex, primitive.levels.size() - 1U);
        const std::size_t previousIndex = std::min(primitive.previousLevelIndex, primitive.levels.size() - 1U);
        const LodSwitchLevel& active = primitive.levels[activeIndex];
        const LodSwitchLevel& previous = primitive.levels[previousIndex];
        states.push_back(LodSwitchSelectionState{
            .id = primitive.id,
            .activeLevel = active.name,
            .previousLevel = previous.name,
            .collisionProfile = active.collisionProfile,
            .crossfadeActive = primitive.policy.transitionMode == LodSwitchTransitionMode::Crossfade
                && primitive.crossfadeAlpha < 1.0f,
            .crossfadeAlpha = primitive.crossfadeAlpha,
            .switchCount = primitive.switchCount,
            .thrashWarnings = primitive.thrashWarnings,
        });
    }
    return states;
}

std::vector<const SurfaceScatterVolume*> RuntimeEnvironmentService::GetSurfaceScatterVolumesAt(
    const ri::math::Vec3& position) const {
    struct Candidate {
        const SurfaceScatterVolume* volume = nullptr;
        float distance = 0.0f;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(surfaceScatterVolumes_.size());
    for (const SurfaceScatterVolume& volume : surfaceScatterVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        candidates.push_back(Candidate{
            .volume = &volume,
            .distance = DistanceToVolumeBounds(position, volume),
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.volume->density.maxPoints != rhs.volume->density.maxPoints) {
            return lhs.volume->density.maxPoints > rhs.volume->density.maxPoints;
        }
        return lhs.distance < rhs.distance;
    });

    std::vector<const SurfaceScatterVolume*> matches;
    matches.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        matches.push_back(candidate.volume);
    }
    return matches;
}

const SurfaceScatterVolume* RuntimeEnvironmentService::GetSurfaceScatterVolumeAt(const ri::math::Vec3& position) const {
    const std::vector<const SurfaceScatterVolume*> matches = GetSurfaceScatterVolumesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<SurfaceScatterRuntimeState> RuntimeEnvironmentService::GetSurfaceScatterRuntimeStates(
    const ri::math::Vec3& viewerPosition) const {
    std::vector<SurfaceScatterRuntimeState> states;
    states.reserve(surfaceScatterVolumes_.size());

    for (const SurfaceScatterVolume& volume : surfaceScatterVolumes_) {
        const float viewerDistance = ri::math::Distance(viewerPosition, volume.position);
        const bool withinDistance = viewerDistance <= volume.culling.maxActiveDistance;
        const bool targetsResolved = !volume.targetIds.empty();
        const std::uint32_t generatedCount = targetsResolved ? ComputeSurfaceScatterGeneratedCount(volume) : 0U;
        const bool active = withinDistance && generatedCount > 0U;
        const float area = std::max(0.01f, std::fabs(volume.size.x * volume.size.z));
        const std::uint32_t densityDrivenCount = volume.density.densityPerSquareMeter > 0.0f
            ? static_cast<std::uint32_t>(std::round(volume.density.densityPerSquareMeter * area))
            : 0U;
        const std::uint32_t requestedCount = std::max(volume.density.count, densityDrivenCount);

        states.push_back(SurfaceScatterRuntimeState{
            .id = volume.id,
            .active = active,
            .withinDistance = withinDistance,
            .targetsResolved = targetsResolved,
            .requestedCount = requestedCount,
            .generatedCount = generatedCount,
            .layoutSignature = HashSurfaceScatterSignature(volume, generatedCount),
            .collisionPolicy = volume.collisionPolicy,
        });
    }

    std::sort(states.begin(), states.end(), [](const SurfaceScatterRuntimeState& lhs, const SurfaceScatterRuntimeState& rhs) {
        if (lhs.active != rhs.active) {
            return lhs.active && !rhs.active;
        }
        if (lhs.generatedCount != rhs.generatedCount) {
            return lhs.generatedCount > rhs.generatedCount;
        }
        return lhs.id < rhs.id;
    });

    return states;
}

std::vector<const SplineMeshDeformerPrimitive*> RuntimeEnvironmentService::GetSplineMeshDeformerPrimitivesAt(
    const ri::math::Vec3& position) const {
    struct Candidate {
        const SplineMeshDeformerPrimitive* primitive = nullptr;
        float distance = 0.0f;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(splineMeshDeformerPrimitives_.size());
    for (const SplineMeshDeformerPrimitive& primitive : splineMeshDeformerPrimitives_) {
        if (!IsPointInsideVolume(position, primitive)) {
            continue;
        }
        candidates.push_back(Candidate{
            .primitive = &primitive,
            .distance = DistanceToVolumeBounds(position, primitive),
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.primitive->sampleCount != rhs.primitive->sampleCount) {
            return lhs.primitive->sampleCount > rhs.primitive->sampleCount;
        }
        return lhs.distance < rhs.distance;
    });

    std::vector<const SplineMeshDeformerPrimitive*> matches;
    matches.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        matches.push_back(candidate.primitive);
    }
    return matches;
}

const SplineMeshDeformerPrimitive* RuntimeEnvironmentService::GetSplineMeshDeformerPrimitiveAt(
    const ri::math::Vec3& position) const {
    const std::vector<const SplineMeshDeformerPrimitive*> matches = GetSplineMeshDeformerPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<SplineMeshDeformerRuntimeState> RuntimeEnvironmentService::GetSplineMeshDeformerRuntimeStates(
    const ri::math::Vec3& viewerPosition) const {
    std::vector<SplineMeshDeformerRuntimeState> states;
    states.reserve(splineMeshDeformerPrimitives_.size());

    for (const SplineMeshDeformerPrimitive& primitive : splineMeshDeformerPrimitives_) {
        const float viewerDistance = ri::math::Distance(viewerPosition, primitive.position);
        const bool withinDistance = viewerDistance <= primitive.maxActiveDistance;
        const bool splineValid = HasValidSplinePoints(primitive.splinePoints);
        const bool targetsResolved = !primitive.targetIds.empty();
        const std::uint32_t requestedSamples = std::clamp<std::uint32_t>(primitive.sampleCount, 2U, primitive.maxSamples);
        const std::uint32_t generatedSegments = (splineValid && targetsResolved && requestedSamples > 1U)
            ? (requestedSamples - 1U)
            : 0U;
        states.push_back(SplineMeshDeformerRuntimeState{
            .id = primitive.id,
            .active = withinDistance && generatedSegments > 0U,
            .withinDistance = withinDistance,
            .splineValid = splineValid,
            .targetsResolved = targetsResolved,
            .requestedSamples = requestedSamples,
            .generatedSegments = generatedSegments,
            .topologySignature = HashSplineTopology(
                primitive.id,
                primitive.splinePoints,
                primitive.seed,
                requestedSamples),
        });
    }

    std::sort(states.begin(), states.end(), [](const SplineMeshDeformerRuntimeState& lhs, const SplineMeshDeformerRuntimeState& rhs) {
        if (lhs.active != rhs.active) {
            return lhs.active && !rhs.active;
        }
        if (lhs.generatedSegments != rhs.generatedSegments) {
            return lhs.generatedSegments > rhs.generatedSegments;
        }
        return lhs.id < rhs.id;
    });

    return states;
}

std::vector<const SplineDecalRibbonPrimitive*> RuntimeEnvironmentService::GetSplineDecalRibbonPrimitivesAt(
    const ri::math::Vec3& position) const {
    struct Candidate {
        const SplineDecalRibbonPrimitive* primitive = nullptr;
        float distance = 0.0f;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(splineDecalRibbonPrimitives_.size());
    for (const SplineDecalRibbonPrimitive& primitive : splineDecalRibbonPrimitives_) {
        if (!IsPointInsideVolume(position, primitive)) {
            continue;
        }
        candidates.push_back(Candidate{
            .primitive = &primitive,
            .distance = DistanceToVolumeBounds(position, primitive),
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.primitive->tessellation != rhs.primitive->tessellation) {
            return lhs.primitive->tessellation > rhs.primitive->tessellation;
        }
        return lhs.distance < rhs.distance;
    });

    std::vector<const SplineDecalRibbonPrimitive*> matches;
    matches.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        matches.push_back(candidate.primitive);
    }
    return matches;
}

const SplineDecalRibbonPrimitive* RuntimeEnvironmentService::GetSplineDecalRibbonPrimitiveAt(
    const ri::math::Vec3& position) const {
    const std::vector<const SplineDecalRibbonPrimitive*> matches = GetSplineDecalRibbonPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<SplineDecalRibbonRuntimeState> RuntimeEnvironmentService::GetSplineDecalRibbonRuntimeStates(
    const ri::math::Vec3& viewerPosition) const {
    std::vector<SplineDecalRibbonRuntimeState> states;
    states.reserve(splineDecalRibbonPrimitives_.size());

    for (const SplineDecalRibbonPrimitive& primitive : splineDecalRibbonPrimitives_) {
        const float viewerDistance = ri::math::Distance(viewerPosition, primitive.position);
        const bool withinDistance = viewerDistance <= primitive.maxActiveDistance;
        const bool splineValid = HasValidSplinePoints(primitive.splinePoints);
        const std::uint32_t requestedSamples = std::clamp<std::uint32_t>(primitive.tessellation, 2U, primitive.maxSamples);
        const std::uint32_t generatedSegments = (splineValid && requestedSamples > 1U) ? (requestedSamples - 1U) : 0U;
        const std::uint32_t generatedTriangles = generatedSegments * 2U;
        states.push_back(SplineDecalRibbonRuntimeState{
            .id = primitive.id,
            .active = withinDistance && generatedSegments > 0U,
            .withinDistance = withinDistance,
            .splineValid = splineValid,
            .requestedSamples = requestedSamples,
            .generatedSegments = generatedSegments,
            .generatedTriangles = generatedTriangles,
            .topologySignature = HashSplineTopology(
                primitive.id,
                primitive.splinePoints,
                primitive.seed,
                requestedSamples),
        });
    }

    std::sort(states.begin(), states.end(), [](const SplineDecalRibbonRuntimeState& lhs, const SplineDecalRibbonRuntimeState& rhs) {
        if (lhs.active != rhs.active) {
            return lhs.active && !rhs.active;
        }
        if (lhs.generatedTriangles != rhs.generatedTriangles) {
            return lhs.generatedTriangles > rhs.generatedTriangles;
        }
        return lhs.id < rhs.id;
    });

    return states;
}

std::vector<const TopologicalUvRemapperVolume*> RuntimeEnvironmentService::GetTopologicalUvRemapperVolumesAt(
    const ri::math::Vec3& position) const {
    struct Candidate {
        const TopologicalUvRemapperVolume* volume = nullptr;
        float distance = 0.0f;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(topologicalUvRemapperVolumes_.size());
    for (const TopologicalUvRemapperVolume& volume : topologicalUvRemapperVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        candidates.push_back(Candidate{
            .volume = &volume,
            .distance = DistanceToVolumeBounds(position, volume),
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.volume->targetIds.size() != rhs.volume->targetIds.size()) {
            return lhs.volume->targetIds.size() > rhs.volume->targetIds.size();
        }
        if (lhs.volume->maxMaterialPatches != rhs.volume->maxMaterialPatches) {
            return lhs.volume->maxMaterialPatches > rhs.volume->maxMaterialPatches;
        }
        return lhs.distance < rhs.distance;
    });

    std::vector<const TopologicalUvRemapperVolume*> matches;
    matches.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        matches.push_back(candidate.volume);
    }
    return matches;
}

const TopologicalUvRemapperVolume* RuntimeEnvironmentService::GetTopologicalUvRemapperVolumeAt(
    const ri::math::Vec3& position) const {
    const std::vector<const TopologicalUvRemapperVolume*> matches = GetTopologicalUvRemapperVolumesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const TriPlanarNode*> RuntimeEnvironmentService::GetTriPlanarNodesAt(const ri::math::Vec3& position) const {
    struct Candidate {
        const TriPlanarNode* node = nullptr;
        float distance = 0.0f;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(triPlanarNodes_.size());
    for (const TriPlanarNode& node : triPlanarNodes_) {
        if (!IsPointInsideVolume(position, node)) {
            continue;
        }
        candidates.push_back(Candidate{
            .node = &node,
            .distance = DistanceToVolumeBounds(position, node),
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.node->targetIds.size() != rhs.node->targetIds.size()) {
            return lhs.node->targetIds.size() > rhs.node->targetIds.size();
        }
        if (lhs.node->blendSharpness != rhs.node->blendSharpness) {
            return lhs.node->blendSharpness > rhs.node->blendSharpness;
        }
        return lhs.distance < rhs.distance;
    });

    std::vector<const TriPlanarNode*> matches;
    matches.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        matches.push_back(candidate.node);
    }
    return matches;
}

const TriPlanarNode* RuntimeEnvironmentService::GetTriPlanarNodeAt(const ri::math::Vec3& position) const {
    const std::vector<const TriPlanarNode*> matches = GetTriPlanarNodesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<ProceduralUvProjectionRuntimeState> RuntimeEnvironmentService::GetProceduralUvProjectionRuntimeStates(
    const ri::math::Vec3& viewerPosition) const {
    std::vector<ProceduralUvProjectionRuntimeState> states;
    states.reserve(topologicalUvRemapperVolumes_.size() + triPlanarNodes_.size());

    for (const TopologicalUvRemapperVolume& volume : topologicalUvRemapperVolumes_) {
        const float viewerDistance = ri::math::Distance(viewerPosition, volume.position);
        const bool withinDistance = viewerDistance <= volume.maxActiveDistance;
        const bool targetSetValid = !volume.targetIds.empty();
        const bool textureSetValid = ProceduralUvTextureSetValid(
            volume.sharedTextureId,
            volume.textureX,
            volume.textureY,
            volume.textureZ);
        const std::uint32_t estimatedPatches = (targetSetValid && textureSetValid)
            ? std::min(volume.maxMaterialPatches, static_cast<std::uint32_t>(volume.targetIds.size()))
            : 0U;
        states.push_back(ProceduralUvProjectionRuntimeState{
            .id = volume.id,
            .kind = ProceduralUvProjectionKind::TopologicalRemapper,
            .active = withinDistance && targetSetValid && textureSetValid,
            .withinDistance = withinDistance,
            .targetSetValid = targetSetValid,
            .textureSetValid = textureSetValid,
            .estimatedMaterialPatches = estimatedPatches,
            .configSignature = HashProceduralUvProjectionConfig(
                0U,
                volume.id,
                volume.remapMode,
                volume.targetIds,
                volume.textureX,
                volume.textureY,
                volume.textureZ,
                volume.sharedTextureId,
                volume.projectionScale,
                volume.blendSharpness,
                volume.axisWeights,
                volume.maxMaterialPatches,
                false,
                volume.debug),
        });
    }

    for (const TriPlanarNode& node : triPlanarNodes_) {
        const float viewerDistance = ri::math::Distance(viewerPosition, node.position);
        const bool withinDistance = viewerDistance <= node.maxActiveDistance;
        const bool targetSetValid = !node.targetIds.empty();
        const bool textureSetValid = ProceduralUvTextureSetValid(
            node.sharedTextureId,
            node.textureX,
            node.textureY,
            node.textureZ);
        const std::uint32_t estimatedPatches = (targetSetValid && textureSetValid)
            ? std::min(node.maxMaterialPatches, static_cast<std::uint32_t>(node.targetIds.size()))
            : 0U;
        states.push_back(ProceduralUvProjectionRuntimeState{
            .id = node.id,
            .kind = ProceduralUvProjectionKind::TriPlanarNode,
            .active = withinDistance && targetSetValid && textureSetValid,
            .withinDistance = withinDistance,
            .targetSetValid = targetSetValid,
            .textureSetValid = textureSetValid,
            .estimatedMaterialPatches = estimatedPatches,
            .configSignature = HashProceduralUvProjectionConfig(
                1U,
                node.id,
                {},
                node.targetIds,
                node.textureX,
                node.textureY,
                node.textureZ,
                node.sharedTextureId,
                node.projectionScale,
                node.blendSharpness,
                node.axisWeights,
                node.maxMaterialPatches,
                node.objectSpaceAxes,
                node.debug),
        });
    }

    std::sort(states.begin(), states.end(), [](const ProceduralUvProjectionRuntimeState& lhs,
                                                const ProceduralUvProjectionRuntimeState& rhs) {
        if (lhs.active != rhs.active) {
            return lhs.active && !rhs.active;
        }
        if (lhs.estimatedMaterialPatches != rhs.estimatedMaterialPatches) {
            return lhs.estimatedMaterialPatches > rhs.estimatedMaterialPatches;
        }
        return lhs.id < rhs.id;
    });

    return states;
}

std::vector<const InstanceCloudPrimitive*> RuntimeEnvironmentService::GetInstanceCloudPrimitivesAt(
    const ri::math::Vec3& position) const {
    struct Candidate {
        const InstanceCloudPrimitive* primitive = nullptr;
        float distance = 0.0f;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(instanceCloudPrimitives_.size());
    for (const InstanceCloudPrimitive& primitive : instanceCloudPrimitives_) {
        if (!IsPointInsideVolume(position, primitive)) {
            continue;
        }
        candidates.push_back(Candidate{
            .primitive = &primitive,
            .distance = DistanceToVolumeBounds(position, primitive),
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.primitive->count != rhs.primitive->count) {
            return lhs.primitive->count > rhs.primitive->count;
        }
        return lhs.distance < rhs.distance;
    });

    std::vector<const InstanceCloudPrimitive*> matches;
    matches.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        matches.push_back(candidate.primitive);
    }
    return matches;
}

const InstanceCloudPrimitive* RuntimeEnvironmentService::GetInstanceCloudPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const InstanceCloudPrimitive*> matches = GetInstanceCloudPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<InstanceCloudRuntimeState> RuntimeEnvironmentService::GetInstanceCloudRuntimeStates(
    const ri::math::Vec3& viewerPosition) const {
    std::vector<InstanceCloudRuntimeState> states;
    states.reserve(instanceCloudPrimitives_.size());

    for (const InstanceCloudPrimitive& primitive : instanceCloudPrimitives_) {
        const float viewerDistance = ri::math::Distance(viewerPosition, primitive.position);
        const bool withinDistance = viewerDistance <= primitive.culling.maxActiveDistance;
        const bool active = withinDistance && primitive.count > 0U;
        states.push_back(InstanceCloudRuntimeState{
            .id = primitive.id,
            .active = active,
            .withinDistance = withinDistance,
            .instanceCount = primitive.count,
            .activeInstanceCount = active ? primitive.count : 0U,
            .viewerDistance = viewerDistance,
            .collisionPolicy = primitive.collisionPolicy,
        });
    }

    std::sort(states.begin(), states.end(), [](const InstanceCloudRuntimeState& lhs, const InstanceCloudRuntimeState& rhs) {
        if (lhs.active != rhs.active) {
            return lhs.active && !rhs.active;
        }
        if (std::fabs(lhs.viewerDistance - rhs.viewerDistance) > 0.0001f) {
            return lhs.viewerDistance < rhs.viewerDistance;
        }
        return lhs.id < rhs.id;
    });

    return states;
}

std::vector<const VoronoiFracturePrimitive*> RuntimeEnvironmentService::GetVoronoiFracturePrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const VoronoiFracturePrimitive*> matches;
    for (const VoronoiFracturePrimitive& primitive : voronoiFracturePrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const VoronoiFracturePrimitive* RuntimeEnvironmentService::GetVoronoiFracturePrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const VoronoiFracturePrimitive*> matches = GetVoronoiFracturePrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const MetaballPrimitive*> RuntimeEnvironmentService::GetMetaballPrimitivesAt(const ri::math::Vec3& position) const {
    std::vector<const MetaballPrimitive*> matches;
    for (const MetaballPrimitive& primitive : metaballPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const MetaballPrimitive* RuntimeEnvironmentService::GetMetaballPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const MetaballPrimitive*> matches = GetMetaballPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const LatticeVolume*> RuntimeEnvironmentService::GetLatticeVolumesAt(const ri::math::Vec3& position) const {
    std::vector<const LatticeVolume*> matches;
    for (const LatticeVolume& volume : latticeVolumes_) {
        if (IsPointInsideVolume(position, volume)) {
            matches.push_back(&volume);
        }
    }
    return matches;
}

const LatticeVolume* RuntimeEnvironmentService::GetLatticeVolumeAt(const ri::math::Vec3& position) const {
    const std::vector<const LatticeVolume*> matches = GetLatticeVolumesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const ManifoldSweepPrimitive*> RuntimeEnvironmentService::GetManifoldSweepPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const ManifoldSweepPrimitive*> matches;
    for (const ManifoldSweepPrimitive& primitive : manifoldSweepPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const ManifoldSweepPrimitive* RuntimeEnvironmentService::GetManifoldSweepPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const ManifoldSweepPrimitive*> matches = GetManifoldSweepPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const TrimSheetSweepPrimitive*> RuntimeEnvironmentService::GetTrimSheetSweepPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const TrimSheetSweepPrimitive*> matches;
    for (const TrimSheetSweepPrimitive& primitive : trimSheetSweepPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const TrimSheetSweepPrimitive* RuntimeEnvironmentService::GetTrimSheetSweepPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const TrimSheetSweepPrimitive*> matches = GetTrimSheetSweepPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const LSystemBranchPrimitive*> RuntimeEnvironmentService::GetLSystemBranchPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const LSystemBranchPrimitive*> matches;
    for (const LSystemBranchPrimitive& primitive : lSystemBranchPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const LSystemBranchPrimitive* RuntimeEnvironmentService::GetLSystemBranchPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const LSystemBranchPrimitive*> matches = GetLSystemBranchPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const GeodesicSpherePrimitive*> RuntimeEnvironmentService::GetGeodesicSpherePrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const GeodesicSpherePrimitive*> matches;
    for (const GeodesicSpherePrimitive& primitive : geodesicSpherePrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const GeodesicSpherePrimitive* RuntimeEnvironmentService::GetGeodesicSpherePrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const GeodesicSpherePrimitive*> matches = GetGeodesicSpherePrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const ExtrudeAlongNormalPrimitive*> RuntimeEnvironmentService::GetExtrudeAlongNormalPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const ExtrudeAlongNormalPrimitive*> matches;
    for (const ExtrudeAlongNormalPrimitive& primitive : extrudeAlongNormalPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const ExtrudeAlongNormalPrimitive* RuntimeEnvironmentService::GetExtrudeAlongNormalPrimitiveAt(
    const ri::math::Vec3& position) const {
    const std::vector<const ExtrudeAlongNormalPrimitive*> matches = GetExtrudeAlongNormalPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const SuperellipsoidPrimitive*> RuntimeEnvironmentService::GetSuperellipsoidPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const SuperellipsoidPrimitive*> matches;
    for (const SuperellipsoidPrimitive& primitive : superellipsoidPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const SuperellipsoidPrimitive* RuntimeEnvironmentService::GetSuperellipsoidPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const SuperellipsoidPrimitive*> matches = GetSuperellipsoidPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const PrimitiveDemoLattice*> RuntimeEnvironmentService::GetPrimitiveDemoLatticePrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const PrimitiveDemoLattice*> matches;
    for (const PrimitiveDemoLattice& primitive : primitiveDemoLatticePrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const PrimitiveDemoLattice* RuntimeEnvironmentService::GetPrimitiveDemoLatticePrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const PrimitiveDemoLattice*> matches = GetPrimitiveDemoLatticePrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const PrimitiveDemoVoronoi*> RuntimeEnvironmentService::GetPrimitiveDemoVoronoiPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const PrimitiveDemoVoronoi*> matches;
    for (const PrimitiveDemoVoronoi& primitive : primitiveDemoVoronoiPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const PrimitiveDemoVoronoi* RuntimeEnvironmentService::GetPrimitiveDemoVoronoiPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const PrimitiveDemoVoronoi*> matches = GetPrimitiveDemoVoronoiPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const ThickPolygonPrimitive*> RuntimeEnvironmentService::GetThickPolygonPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const ThickPolygonPrimitive*> matches;
    for (const ThickPolygonPrimitive& primitive : thickPolygonPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const ThickPolygonPrimitive* RuntimeEnvironmentService::GetThickPolygonPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const ThickPolygonPrimitive*> matches = GetThickPolygonPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const StructuralProfilePrimitive*> RuntimeEnvironmentService::GetStructuralProfilePrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const StructuralProfilePrimitive*> matches;
    for (const StructuralProfilePrimitive& primitive : structuralProfilePrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const StructuralProfilePrimitive* RuntimeEnvironmentService::GetStructuralProfilePrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const StructuralProfilePrimitive*> matches = GetStructuralProfilePrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const HalfPipePrimitive*> RuntimeEnvironmentService::GetHalfPipePrimitivesAt(const ri::math::Vec3& position) const {
    std::vector<const HalfPipePrimitive*> matches;
    for (const HalfPipePrimitive& primitive : halfPipePrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const HalfPipePrimitive* RuntimeEnvironmentService::GetHalfPipePrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const HalfPipePrimitive*> matches = GetHalfPipePrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const QuarterPipePrimitive*> RuntimeEnvironmentService::GetQuarterPipePrimitivesAt(const ri::math::Vec3& position) const {
    std::vector<const QuarterPipePrimitive*> matches;
    for (const QuarterPipePrimitive& primitive : quarterPipePrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const QuarterPipePrimitive* RuntimeEnvironmentService::GetQuarterPipePrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const QuarterPipePrimitive*> matches = GetQuarterPipePrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const PipeElbowPrimitive*> RuntimeEnvironmentService::GetPipeElbowPrimitivesAt(const ri::math::Vec3& position) const {
    std::vector<const PipeElbowPrimitive*> matches;
    for (const PipeElbowPrimitive& primitive : pipeElbowPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const PipeElbowPrimitive* RuntimeEnvironmentService::GetPipeElbowPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const PipeElbowPrimitive*> matches = GetPipeElbowPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const TorusSlicePrimitive*> RuntimeEnvironmentService::GetTorusSlicePrimitivesAt(const ri::math::Vec3& position) const {
    std::vector<const TorusSlicePrimitive*> matches;
    for (const TorusSlicePrimitive& primitive : torusSlicePrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const TorusSlicePrimitive* RuntimeEnvironmentService::GetTorusSlicePrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const TorusSlicePrimitive*> matches = GetTorusSlicePrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const SplineSweepPrimitive*> RuntimeEnvironmentService::GetSplineSweepPrimitivesAt(const ri::math::Vec3& position) const {
    std::vector<const SplineSweepPrimitive*> matches;
    for (const SplineSweepPrimitive& primitive : splineSweepPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const SplineSweepPrimitive* RuntimeEnvironmentService::GetSplineSweepPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const SplineSweepPrimitive*> matches = GetSplineSweepPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const RevolvePrimitive*> RuntimeEnvironmentService::GetRevolvePrimitivesAt(const ri::math::Vec3& position) const {
    std::vector<const RevolvePrimitive*> matches;
    for (const RevolvePrimitive& primitive : revolvePrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const RevolvePrimitive* RuntimeEnvironmentService::GetRevolvePrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const RevolvePrimitive*> matches = GetRevolvePrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const DomeVaultPrimitive*> RuntimeEnvironmentService::GetDomeVaultPrimitivesAt(const ri::math::Vec3& position) const {
    std::vector<const DomeVaultPrimitive*> matches;
    for (const DomeVaultPrimitive& primitive : domeVaultPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const DomeVaultPrimitive* RuntimeEnvironmentService::GetDomeVaultPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const DomeVaultPrimitive*> matches = GetDomeVaultPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const LoftPrimitive*> RuntimeEnvironmentService::GetLoftPrimitivesAt(const ri::math::Vec3& position) const {
    std::vector<const LoftPrimitive*> matches;
    for (const LoftPrimitive& primitive : loftPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

const LoftPrimitive* RuntimeEnvironmentService::GetLoftPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const LoftPrimitive*> matches = GetLoftPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const NavmeshModifierVolume*> RuntimeEnvironmentService::GetNavmeshModifiersAt(const ri::math::Vec3& position) const {
    std::vector<const NavmeshModifierVolume*> matches;
    for (const NavmeshModifierVolume& volume : navmeshModifierVolumes_) {
        if (IsPointInsideVolume(position, volume)) {
            matches.push_back(&volume);
        }
    }
    return matches;
}

NavmeshModifierAggregateState RuntimeEnvironmentService::GetNavmeshModifierAggregateAt(const ri::math::Vec3& position) const {
    NavmeshModifierAggregateState state{};
    state.matches = GetNavmeshModifiersAt(position);
    for (const NavmeshModifierVolume* modifier : state.matches) {
        const float cost = ClampFloatWithFallback(modifier->traversalCost, 1.0f, 0.01f, 100.0f);
        state.traversalCostMultiplier *= cost;
        if (cost > state.maxTraversalCost) {
            state.maxTraversalCost = cost;
            state.dominantTag = modifier->tag.empty() ? std::string("modified") : modifier->tag;
        }
    }
    state.traversalCostMultiplier = ClampFloat(state.traversalCostMultiplier, 0.01f, 1000.0f);
    return state;
}

std::vector<const ReferenceImagePlane*> RuntimeEnvironmentService::GetReferenceImagePlanesAt(
    const ri::math::Vec3& position) const {
    struct Candidate {
        const ReferenceImagePlane* plane = nullptr;
        float distance = 0.0f;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(referenceImagePlanes_.size());
    for (const ReferenceImagePlane& plane : referenceImagePlanes_) {
        if (!IsPointInsideVolume(position, plane)) {
            continue;
        }
        candidates.push_back(Candidate{
            .plane = &plane,
            .distance = DistanceToVolumeBounds(position, plane),
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.plane->renderOrder != rhs.plane->renderOrder) {
            return lhs.plane->renderOrder > rhs.plane->renderOrder;
        }
        if (std::fabs(lhs.plane->opacity - rhs.plane->opacity) > 0.0001f) {
            return lhs.plane->opacity > rhs.plane->opacity;
        }
        return lhs.distance < rhs.distance;
    });

    std::vector<const ReferenceImagePlane*> matches;
    matches.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        matches.push_back(candidate.plane);
    }
    return matches;
}

const ReferenceImagePlane* RuntimeEnvironmentService::GetReferenceImagePlaneAt(const ri::math::Vec3& position) const {
    const std::vector<const ReferenceImagePlane*> matches = GetReferenceImagePlanesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const Text3dPrimitive*> RuntimeEnvironmentService::GetText3dPrimitivesAt(const ri::math::Vec3& position) const {
    struct Candidate {
        const Text3dPrimitive* textPrimitive = nullptr;
        float distance = 0.0f;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(text3dPrimitives_.size());
    for (const Text3dPrimitive& textPrimitive : text3dPrimitives_) {
        if (!IsPointInsideVolume(position, textPrimitive)) {
            continue;
        }
        candidates.push_back(Candidate{
            .textPrimitive = &textPrimitive,
            .distance = DistanceToVolumeBounds(position, textPrimitive),
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (std::fabs(lhs.textPrimitive->textScale - rhs.textPrimitive->textScale) > 0.0001f) {
            return lhs.textPrimitive->textScale > rhs.textPrimitive->textScale;
        }
        return lhs.distance < rhs.distance;
    });

    std::vector<const Text3dPrimitive*> matches;
    matches.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        matches.push_back(candidate.textPrimitive);
    }
    return matches;
}

const Text3dPrimitive* RuntimeEnvironmentService::GetText3dPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const Text3dPrimitive*> matches = GetText3dPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const AnnotationCommentPrimitive*> RuntimeEnvironmentService::GetAnnotationCommentPrimitivesAt(
    const ri::math::Vec3& position) const {
    struct Candidate {
        const AnnotationCommentPrimitive* comment = nullptr;
        float distance = 0.0f;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(annotationCommentPrimitives_.size());
    for (const AnnotationCommentPrimitive& comment : annotationCommentPrimitives_) {
        if (!IsPointInsideVolume(position, comment)) {
            continue;
        }
        candidates.push_back(Candidate{
            .comment = &comment,
            .distance = DistanceToVolumeBounds(position, comment),
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (std::fabs(lhs.comment->textScale - rhs.comment->textScale) > 0.0001f) {
            return lhs.comment->textScale > rhs.comment->textScale;
        }
        return lhs.distance < rhs.distance;
    });

    std::vector<const AnnotationCommentPrimitive*> matches;
    matches.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        matches.push_back(candidate.comment);
    }
    return matches;
}

const AnnotationCommentPrimitive* RuntimeEnvironmentService::GetAnnotationCommentPrimitiveAt(
    const ri::math::Vec3& position) const {
    const std::vector<const AnnotationCommentPrimitive*> matches = GetAnnotationCommentPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const MeasureToolPrimitive*> RuntimeEnvironmentService::GetMeasureToolPrimitivesAt(
    const ri::math::Vec3& position) const {
    struct Candidate {
        const MeasureToolPrimitive* tool = nullptr;
        float distance = 0.0f;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(measureToolPrimitives_.size());
    for (const MeasureToolPrimitive& tool : measureToolPrimitives_) {
        if (!IsPointInsideVolume(position, tool)) {
            continue;
        }
        candidates.push_back(Candidate{
            .tool = &tool,
            .distance = DistanceToVolumeBounds(position, tool),
        });
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (std::fabs(lhs.tool->textScale - rhs.tool->textScale) > 0.0001f) {
            return lhs.tool->textScale > rhs.tool->textScale;
        }
        return lhs.distance < rhs.distance;
    });

    std::vector<const MeasureToolPrimitive*> matches;
    matches.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        matches.push_back(candidate.tool);
    }
    return matches;
}

const MeasureToolPrimitive* RuntimeEnvironmentService::GetMeasureToolPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const MeasureToolPrimitive*> matches = GetMeasureToolPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<MeasureToolReadout> RuntimeEnvironmentService::GetMeasureToolReadoutsAt(const ri::math::Vec3& position) const {
    const std::vector<const MeasureToolPrimitive*> activeTools = GetMeasureToolPrimitivesAt(position);
    std::vector<MeasureToolReadout> readouts;
    readouts.reserve(activeTools.size());

    for (const MeasureToolPrimitive* tool : activeTools) {
        if (tool == nullptr) {
            continue;
        }

        MeasureToolReadout readout{};
        readout.id = tool->id;
        readout.mode = tool->mode;
        readout.showFill = tool->showFill;
        readout.showWireframe = tool->showWireframe;

        if (tool->mode == MeasureToolMode::Line) {
            const ri::math::Vec3 midpoint = (tool->lineStart + tool->lineEnd) * 0.5f;
            const double length = static_cast<double>(ri::math::Length(tool->lineEnd - tool->lineStart));
            readout.primaryValue = length;
            readout.labelPosition = midpoint + tool->labelOffset;
            readout.label = FormatMeasureScalar(length) + tool->unitSuffix;
        } else {
            const ri::math::Vec3 dimensions{
                std::fabs(tool->size.x),
                std::fabs(tool->size.y),
                std::fabs(tool->size.z),
            };
            readout.dimensions = dimensions;
            readout.primaryValue = static_cast<double>(ri::math::Length(dimensions));
            readout.labelPosition = ri::math::Vec3{
                tool->position.x + tool->labelOffset.x,
                tool->position.y + (dimensions.y * 0.5f) + tool->labelOffset.y,
                tool->position.z + tool->labelOffset.z,
            };
            readout.label = FormatMeasureScalar(dimensions.x) + " x "
                + FormatMeasureScalar(dimensions.y) + " x "
                + FormatMeasureScalar(dimensions.z) + tool->unitSuffix;
        }

        readouts.push_back(std::move(readout));
    }

    return readouts;
}

std::vector<const RenderTargetSurface*> RuntimeEnvironmentService::GetRenderTargetSurfacesAt(
    const ri::math::Vec3& position) const {
    std::vector<const RenderTargetSurface*> matches;
    matches.reserve(renderTargetSurfaces_.size());
    for (const RenderTargetSurface& surface : renderTargetSurfaces_) {
        if (IsPointInsideVolume(position, surface)) {
            matches.push_back(&surface);
        }
    }
    std::sort(matches.begin(), matches.end(), [](const RenderTargetSurface* lhs, const RenderTargetSurface* rhs) {
        const int lhsResolution = ResolveEffectiveSurfaceResolution(lhs->renderResolution, lhs->resolutionCap);
        const int rhsResolution = ResolveEffectiveSurfaceResolution(rhs->renderResolution, rhs->resolutionCap);
        if (lhsResolution != rhsResolution) {
            return lhsResolution > rhsResolution;
        }
        return lhs->updateEveryFrames < rhs->updateEveryFrames;
    });
    return matches;
}

const RenderTargetSurface* RuntimeEnvironmentService::GetRenderTargetSurfaceAt(const ri::math::Vec3& position) const {
    const std::vector<const RenderTargetSurface*> matches = GetRenderTargetSurfacesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<const PlanarReflectionSurface*> RuntimeEnvironmentService::GetPlanarReflectionSurfacesAt(
    const ri::math::Vec3& position) const {
    std::vector<const PlanarReflectionSurface*> matches;
    matches.reserve(planarReflectionSurfaces_.size());
    for (const PlanarReflectionSurface& surface : planarReflectionSurfaces_) {
        if (IsPointInsideVolume(position, surface)) {
            matches.push_back(&surface);
        }
    }
    std::sort(matches.begin(), matches.end(), [](const PlanarReflectionSurface* lhs, const PlanarReflectionSurface* rhs) {
        const int lhsResolution = ResolveEffectiveSurfaceResolution(lhs->renderResolution, lhs->resolutionCap);
        const int rhsResolution = ResolveEffectiveSurfaceResolution(rhs->renderResolution, rhs->resolutionCap);
        if (lhsResolution != rhsResolution) {
            return lhsResolution > rhsResolution;
        }
        return lhs->updateEveryFrames < rhs->updateEveryFrames;
    });
    return matches;
}

const PlanarReflectionSurface* RuntimeEnvironmentService::GetPlanarReflectionSurfaceAt(
    const ri::math::Vec3& position) const {
    const std::vector<const PlanarReflectionSurface*> matches = GetPlanarReflectionSurfacesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<DynamicSurfaceRenderState> RuntimeEnvironmentService::GetDynamicSurfaceRenderStates(
    const ri::math::Vec3& viewerPosition,
    std::uint64_t frameIndex) const {
    std::vector<DynamicSurfaceRenderState> states;
    states.reserve(renderTargetSurfaces_.size() + planarReflectionSurfaces_.size());

    for (const RenderTargetSurface& surface : renderTargetSurfaces_) {
        const float viewerDistance = ri::math::Distance(viewerPosition, surface.position);
        const bool withinDistanceGate = !surface.enableDistanceGate || viewerDistance <= surface.maxActiveDistance;
        states.push_back(DynamicSurfaceRenderState{
            .id = surface.id,
            .kind = DynamicSurfaceKind::RenderTarget,
            .insideVolume = IsPointInsideVolume(viewerPosition, surface),
            .withinDistanceGate = withinDistanceGate,
            .shouldUpdate = withinDistanceGate && IsSurfaceUpdateFrame(frameIndex, surface.updateEveryFrames),
            .effectiveResolution = ResolveEffectiveSurfaceResolution(surface.renderResolution, surface.resolutionCap),
            .updateEveryFrames = std::clamp<std::uint32_t>(surface.updateEveryFrames, 1U, 120U),
            .viewerDistance = viewerDistance,
        });
    }

    for (const PlanarReflectionSurface& surface : planarReflectionSurfaces_) {
        const float viewerDistance = ri::math::Distance(viewerPosition, surface.position);
        const bool withinDistanceGate = !surface.enableDistanceGate || viewerDistance <= surface.maxActiveDistance;
        states.push_back(DynamicSurfaceRenderState{
            .id = surface.id,
            .kind = DynamicSurfaceKind::PlanarReflection,
            .insideVolume = IsPointInsideVolume(viewerPosition, surface),
            .withinDistanceGate = withinDistanceGate,
            .shouldUpdate = withinDistanceGate && IsSurfaceUpdateFrame(frameIndex, surface.updateEveryFrames),
            .effectiveResolution = ResolveEffectiveSurfaceResolution(surface.renderResolution, surface.resolutionCap),
            .updateEveryFrames = std::clamp<std::uint32_t>(surface.updateEveryFrames, 1U, 120U),
            .viewerDistance = viewerDistance,
        });
    }

    std::sort(states.begin(), states.end(), [](const DynamicSurfaceRenderState& lhs, const DynamicSurfaceRenderState& rhs) {
        if (lhs.shouldUpdate != rhs.shouldUpdate) {
            return lhs.shouldUpdate;
        }
        if (lhs.effectiveResolution != rhs.effectiveResolution) {
            return lhs.effectiveResolution > rhs.effectiveResolution;
        }
        return lhs.viewerDistance < rhs.viewerDistance;
    });
    return states;
}

std::vector<const PassThroughPrimitive*> RuntimeEnvironmentService::GetPassThroughPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const PassThroughPrimitive*> matches;
    matches.reserve(passThroughPrimitives_.size());
    for (const PassThroughPrimitive& primitive : passThroughPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }

    std::sort(matches.begin(), matches.end(), [](const PassThroughPrimitive* lhs, const PassThroughPrimitive* rhs) {
        if (std::fabs(lhs->material.opacity - rhs->material.opacity) > 0.0001f) {
            return lhs->material.opacity > rhs->material.opacity;
        }
        return lhs->interactionProfile.raycastSelectable && !rhs->interactionProfile.raycastSelectable;
    });
    return matches;
}

const PassThroughPrimitive* RuntimeEnvironmentService::GetPassThroughPrimitiveAt(const ri::math::Vec3& position) const {
    const std::vector<const PassThroughPrimitive*> matches = GetPassThroughPrimitivesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<PassThroughVisualState> RuntimeEnvironmentService::GetPassThroughVisualStates(
    const ri::math::Vec3& viewerPosition,
    double timeSeconds) const {
    std::vector<PassThroughVisualState> states;
    states.reserve(passThroughPrimitives_.size());
    for (const PassThroughPrimitive& primitive : passThroughPrimitives_) {
        const bool blocksAnyChannel = primitive.interactionProfile.blocksPlayer
            || primitive.interactionProfile.blocksNpc
            || primitive.interactionProfile.blocksProjectiles;
        const float effectiveOpacity = ComputePassThroughOpacity(primitive, viewerPosition, timeSeconds);
        const bool hasConfiguredEvents = !primitive.events.onEnter.empty()
            || !primitive.events.onExit.empty()
            || !primitive.events.onUse.empty();
        states.push_back(PassThroughVisualState{
            .id = primitive.id,
            .effectiveOpacity = effectiveOpacity,
            .hasPulse = primitive.visualBehavior.pulseEnabled,
            .hasDistanceFade = primitive.visualBehavior.distanceFadeEnabled,
            .hasConfiguredEvents = hasConfiguredEvents,
            .blocksAnyChannel = blocksAnyChannel,
            .invisibleWallRisk = blocksAnyChannel && effectiveOpacity <= 0.25f,
        });
    }
    return states;
}

std::vector<const SkyProjectionSurface*> RuntimeEnvironmentService::GetSkyProjectionSurfacesAt(
    const ri::math::Vec3& position) const {
    std::vector<const SkyProjectionSurface*> matches;
    matches.reserve(skyProjectionSurfaces_.size());
    for (const SkyProjectionSurface& surface : skyProjectionSurfaces_) {
        if (IsPointInsideVolume(position, surface)) {
            matches.push_back(&surface);
        }
    }
    std::sort(matches.begin(), matches.end(), [](const SkyProjectionSurface* lhs, const SkyProjectionSurface* rhs) {
        if (lhs->behavior.renderLayer != rhs->behavior.renderLayer) {
            return static_cast<int>(lhs->behavior.renderLayer) < static_cast<int>(rhs->behavior.renderLayer);
        }
        return lhs->visual.opacity > rhs->visual.opacity;
    });
    return matches;
}

const SkyProjectionSurface* RuntimeEnvironmentService::GetSkyProjectionSurfaceAt(const ri::math::Vec3& position) const {
    const std::vector<const SkyProjectionSurface*> matches = GetSkyProjectionSurfacesAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<SkyProjectionSurfaceState> RuntimeEnvironmentService::GetSkyProjectionSurfaceStates(
    const ri::math::Vec3& cameraPosition,
    float cameraYawRadians) const {
    std::vector<SkyProjectionSurfaceState> states;
    states.reserve(skyProjectionSurfaces_.size());
    for (const SkyProjectionSurface& surface : skyProjectionSurfaces_) {
        const ri::math::Vec3 parallaxOffset = ComputeSkyProjectionParallaxOffset(surface, cameraPosition);
        const ri::math::Vec3 projectedPosition = surface.position + parallaxOffset;
        const float projectedYaw = surface.behavior.followCameraYaw ? cameraYawRadians : 0.0f;
        states.push_back(SkyProjectionSurfaceState{
            .id = surface.id,
            .projectedPosition = projectedPosition,
            .projectedYawRadians = projectedYaw,
            .effectiveOpacity = ClampFloatWithFallback(surface.visual.opacity, 1.0f, 0.0f, 1.0f),
            .usesTexture = surface.visual.mode == SkyProjectionVisualMode::Texture && !surface.visual.textureId.empty(),
            .backgroundLayer = surface.behavior.renderLayer == SkyProjectionRenderLayer::Background,
            .requiresPerFrameUpdate = surface.behavior.followCameraYaw
                || surface.behavior.parallaxFactor > 0.0001f
                || surface.behavior.distanceLock,
        });
    }
    return states;
}

std::vector<const VolumetricEmitterBounds*> RuntimeEnvironmentService::GetVolumetricEmitterBoundsAt(
    const ri::math::Vec3& position) const {
    std::vector<const VolumetricEmitterBounds*> matches;
    matches.reserve(volumetricEmitterBounds_.size());
    for (const VolumetricEmitterBounds& volume : volumetricEmitterBounds_) {
        if (IsPointInsideVolume(position, volume)) {
            matches.push_back(&volume);
        }
    }

    std::sort(matches.begin(), matches.end(), [&position](const VolumetricEmitterBounds* lhs, const VolumetricEmitterBounds* rhs) {
        const float lhsDistance = DistanceToVolumeBounds(position, *lhs);
        const float rhsDistance = DistanceToVolumeBounds(position, *rhs);
        if (std::fabs(lhsDistance - rhsDistance) > 0.0001f) {
            return lhsDistance < rhsDistance;
        }
        if (lhs->emission.particleCount != rhs->emission.particleCount) {
            return lhs->emission.particleCount > rhs->emission.particleCount;
        }
        return lhs->id < rhs->id;
    });
    return matches;
}

const VolumetricEmitterBounds* RuntimeEnvironmentService::GetVolumetricEmitterBoundsAtPoint(const ri::math::Vec3& position) const {
    const std::vector<const VolumetricEmitterBounds*> matches = GetVolumetricEmitterBoundsAt(position);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<VolumetricEmitterRuntimeState> RuntimeEnvironmentService::GetVolumetricEmitterRuntimeStates(
    const ri::math::Vec3& viewerPosition) const {
    std::vector<VolumetricEmitterRuntimeState> states;
    states.reserve(volumetricEmitterBounds_.size());
    for (const VolumetricEmitterBounds& volume : volumetricEmitterBounds_) {
        const float viewerDistance = ri::math::Distance(viewerPosition, volume.position);
        const bool insideVolume = IsPointInsideVolume(viewerPosition, volume);

        const ri::world::ParticleSpawnAuthoring* spawn =
            volume.particleSpawn.has_value() ? &*volume.particleSpawn : nullptr;

        const float configuredRangeLimit = (spawn != nullptr && spawn->activation.outerProximityRadius > 0.0f)
            ? spawn->activation.outerProximityRadius
            : volume.culling.maxActiveDistance;
        const float rangeLimit = ClampFloatWithFallback(configuredRangeLimit, 1.0f, 0.0001f, 100000.0f);
        const bool withinRange = viewerDistance <= rangeLimit;

        const bool emissionActive =
            volume.emission.loop
            || volume.emission.spawnRatePerSecond > 0.0001f
            || (spawn != nullptr && spawn->emissionPolicy.burstCountOnEnter > 0U);

        bool shouldSimulate = false;
        if (spawn != nullptr && spawn->activation.alwaysOnAmbient) {
            shouldSimulate = emissionActive;
        } else if (spawn != nullptr && spawn->activation.strictInnerVolumeOnly) {
            shouldSimulate = insideVolume && withinRange && emissionActive;
        } else {
            shouldSimulate = withinRange
                && (!volume.culling.pauseWhenOffscreen || insideVolume)
                && emissionActive;
        }

        const float distanceRatio = rangeLimit > 0.0001f
            ? ClampFloat(1.0f - (viewerDistance / rangeLimit), 0.0f, 1.0f)
            : 0.0f;
        const float qualityScale = 0.25f + (distanceRatio * 0.75f);
        const std::uint32_t scaledCount = static_cast<std::uint32_t>(std::max(
            8.0,
            std::round(static_cast<double>(volume.emission.particleCount) * qualityScale)));

        states.push_back(VolumetricEmitterRuntimeState{
            .id = volume.id,
            .shouldSimulate = shouldSimulate,
            .withinDistanceGate = withinRange,
            .insideVolume = insideVolume,
            .viewerDistance = viewerDistance,
            .effectiveParticleCount = scaledCount,
            .spawnRatePerSecond = volume.emission.spawnRatePerSecond,
            .loop = volume.emission.loop,
            .effectiveProximityRadius = rangeLimit,
            .strictInnerVolumeOnly = spawn != nullptr && spawn->activation.strictInnerVolumeOnly,
            .alwaysOnAmbient = spawn != nullptr && spawn->activation.alwaysOnAmbient,
            .burstOnEnterCount = spawn != nullptr ? spawn->emissionPolicy.burstCountOnEnter : 0U,
            .oneShot = spawn != nullptr && spawn->emissionPolicy.oneShot,
        });
    }

    std::sort(states.begin(), states.end(), [](const VolumetricEmitterRuntimeState& lhs, const VolumetricEmitterRuntimeState& rhs) {
        if (lhs.shouldSimulate != rhs.shouldSimulate) {
            return lhs.shouldSimulate;
        }
        if (lhs.effectiveParticleCount != rhs.effectiveParticleCount) {
            return lhs.effectiveParticleCount > rhs.effectiveParticleCount;
        }
        if (std::fabs(lhs.viewerDistance - rhs.viewerDistance) > 0.0001f) {
            return lhs.viewerDistance < rhs.viewerDistance;
        }
        return lhs.id < rhs.id;
    });
    return states;
}

std::vector<const SplinePathFollowerPrimitive*> RuntimeEnvironmentService::GetSplinePathFollowerPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const SplinePathFollowerPrimitive*> matches;
    matches.reserve(splinePathFollowerPrimitives_.size());
    for (const SplinePathFollowerPrimitive& primitive : splinePathFollowerPrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

std::vector<const CablePrimitive*> RuntimeEnvironmentService::GetCablePrimitivesAt(const ri::math::Vec3& position) const {
    std::vector<const CablePrimitive*> matches;
    matches.reserve(cablePrimitives_.size());
    for (const CablePrimitive& primitive : cablePrimitives_) {
        if (IsPointInsideVolume(position, primitive)) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

std::vector<SplinePathFollowerRuntimeState> RuntimeEnvironmentService::GetSplinePathFollowerRuntimeStates(
    const ri::math::Vec3& viewerPosition,
    double timeSeconds) const {
    std::vector<SplinePathFollowerRuntimeState> states;
    states.reserve(splinePathFollowerPrimitives_.size());
    for (const SplinePathFollowerPrimitive& primitive : splinePathFollowerPrimitives_) {
        SplinePathFollowerRuntimeState state{};
        state.id = primitive.id;
        state.active = IsPointInsideVolume(viewerPosition, primitive);
        state.splineValid = primitive.splinePoints.size() >= 2U;
        state.loop = primitive.loop;
        state.speedUnitsPerSecond = primitive.speedUnitsPerSecond;
        const float tRaw = static_cast<float>(timeSeconds * static_cast<double>(primitive.speedUnitsPerSecond))
            + primitive.phaseOffset;
        float normalizedProgress = primitive.loop ? std::fmod(tRaw, 1.0f) : tRaw;
        if (normalizedProgress < 0.0f) {
            normalizedProgress += 1.0f;
        }
        normalizedProgress = primitive.loop ? normalizedProgress : ClampFloat(normalizedProgress, 0.0f, 1.0f);
        state.normalizedProgress = normalizedProgress;
        const auto [samplePosition, sampleForward] =
            SampleSplinePathFollower(primitive.splinePoints, primitive.loop, normalizedProgress);
        state.samplePosition = samplePosition;
        state.sampleForward = sampleForward;
        states.push_back(state);
    }
    std::sort(states.begin(), states.end(), [&](const SplinePathFollowerRuntimeState& lhs, const SplinePathFollowerRuntimeState& rhs) {
        if (lhs.active != rhs.active) {
            return lhs.active;
        }
        const float lhsDistance = ri::math::Distance(lhs.samplePosition, viewerPosition);
        const float rhsDistance = ri::math::Distance(rhs.samplePosition, viewerPosition);
        if (std::fabs(lhsDistance - rhsDistance) > 0.0001f) {
            return lhsDistance < rhsDistance;
        }
        return lhs.id < rhs.id;
    });
    return states;
}

std::vector<CableRuntimeState> RuntimeEnvironmentService::GetCableRuntimeStates(
    const ri::math::Vec3& viewerPosition,
    double timeSeconds) const {
    std::vector<CableRuntimeState> states;
    states.reserve(cablePrimitives_.size());
    for (const CablePrimitive& primitive : cablePrimitives_) {
        CableRuntimeState state{};
        state.id = primitive.id;
        state.active = IsPointInsideVolume(viewerPosition, primitive);
        state.collisionEnabled = primitive.collisionEnabled;
        state.resolvedStart = primitive.position + primitive.start;
        state.resolvedEnd = primitive.position + primitive.end;
        const double phase = (timeSeconds * static_cast<double>(primitive.swayFrequency)) * 6.283185307179586;
        state.swayOffset = static_cast<float>(std::sin(phase)) * primitive.swayAmplitude;
        state.resolvedEnd.x += state.swayOffset;
        states.push_back(state);
    }
    std::sort(states.begin(), states.end(), [&](const CableRuntimeState& lhs, const CableRuntimeState& rhs) {
        if (lhs.active != rhs.active) {
            return lhs.active;
        }
        const float lhsDistance = ri::math::Distance(lhs.resolvedStart, viewerPosition);
        const float rhsDistance = ri::math::Distance(rhs.resolvedStart, viewerPosition);
        if (std::fabs(lhsDistance - rhsDistance) > 0.0001f) {
            return lhsDistance < rhsDistance;
        }
        return lhs.id < rhs.id;
    });
    return states;
}

std::vector<const ClippingRuntimeVolume*> RuntimeEnvironmentService::GetClippingVolumesAt(const ri::math::Vec3& position) const {
    std::vector<const ClippingRuntimeVolume*> matches;
    matches.reserve(clippingVolumes_.size());
    for (const ClippingRuntimeVolume& volume : clippingVolumes_) {
        if (volume.enabled && IsPointInsideVolume(position, volume)) {
            matches.push_back(&volume);
        }
    }
    return matches;
}

std::vector<const FilteredCollisionRuntimeVolume*> RuntimeEnvironmentService::GetFilteredCollisionVolumesAt(
    const ri::math::Vec3& position) const {
    std::vector<const FilteredCollisionRuntimeVolume*> matches;
    matches.reserve(filteredCollisionVolumes_.size());
    for (const FilteredCollisionRuntimeVolume& volume : filteredCollisionVolumes_) {
        if (IsPointInsideVolume(position, volume)) {
            matches.push_back(&volume);
        }
    }
    return matches;
}

std::vector<const VisibilityPrimitive*> RuntimeEnvironmentService::GetVisibilityPrimitivesAt(
    const ri::math::Vec3& position) const {
    std::vector<const VisibilityPrimitive*> matches;
    for (const VisibilityPrimitive& primitive : visibilityPrimitives_) {
        const ri::math::Vec3 halfExtents{
            std::abs(primitive.size.x) * 0.5f,
            std::abs(primitive.size.y) * 0.5f,
            std::abs(primitive.size.z) * 0.5f,
        };
        const ri::math::Vec3 min = primitive.position - halfExtents;
        const ri::math::Vec3 max = primitive.position + halfExtents;
        const bool inside = position.x >= min.x && position.y >= min.y && position.z >= min.z
            && position.x <= max.x && position.y <= max.y && position.z <= max.z;
        if (inside) {
            matches.push_back(&primitive);
        }
    }
    return matches;
}

std::vector<const GenericTriggerVolume*> RuntimeEnvironmentService::GetGenericTriggerVolumesAt(
    const ri::math::Vec3& position) const {
    std::vector<const GenericTriggerVolume*> matches;
    for (const GenericTriggerVolume& volume : genericTriggerVolumes_) {
        if (IsPointInsideVolume(position, volume)) {
            matches.push_back(&volume);
        }
    }
    return matches;
}

SpatialQueryMatchState RuntimeEnvironmentService::GetSpatialQueryStateAt(const ri::math::Vec3& position,
                                                                         std::uint32_t requiredFilterMask) const {
    SpatialQueryMatchState state{};
    for (const SpatialQueryVolume& volume : spatialQueryVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        if (volume.filterMask == 0U) {
            state.hasUnfilteredVolume = true;
        }
        state.combinedFilterMask |= volume.filterMask;
        const bool include = requiredFilterMask == 0U
            || volume.filterMask == 0U
            || (volume.filterMask & requiredFilterMask) != 0U;
        if (include) {
            state.matches.push_back(&volume);
        }
    }
    return state;
}

std::vector<const SpatialQueryVolume*> RuntimeEnvironmentService::GetSpatialQueryVolumesAt(
    const ri::math::Vec3& position) const {
    std::vector<const SpatialQueryVolume*> matches;
    for (const SpatialQueryVolume& volume : spatialQueryVolumes_) {
        if (IsPointInsideVolume(position, volume)) {
            matches.push_back(&volume);
        }
    }
    return matches;
}

const StreamingLevelVolume* RuntimeEnvironmentService::GetStreamingLevelVolumeAt(const ri::math::Vec3& position) const {
    auto it = std::find_if(
        streamingLevelVolumes_.begin(),
        streamingLevelVolumes_.end(),
        [&position](const StreamingLevelVolume& volume) {
            return IsPointInsideVolume(position, volume);
        });
    return it == streamingLevelVolumes_.end() ? nullptr : &(*it);
}

const CheckpointSpawnVolume* RuntimeEnvironmentService::GetCheckpointSpawnVolumeAt(const ri::math::Vec3& position) const {
    auto it = std::find_if(
        checkpointSpawnVolumes_.begin(),
        checkpointSpawnVolumes_.end(),
        [&position](const CheckpointSpawnVolume& volume) {
            return IsPointInsideVolume(position, volume);
        });
    return it == checkpointSpawnVolumes_.end() ? nullptr : &(*it);
}

const TeleportVolume* RuntimeEnvironmentService::GetTeleportVolumeAt(const ri::math::Vec3& position) const {
    auto it = std::find_if(
        teleportVolumes_.begin(),
        teleportVolumes_.end(),
        [&position](const TeleportVolume& volume) {
            return IsPointInsideVolume(position, volume);
        });
    return it == teleportVolumes_.end() ? nullptr : &(*it);
}

const LaunchVolume* RuntimeEnvironmentService::GetLaunchVolumeAt(const ri::math::Vec3& position) const {
    auto it = std::find_if(
        launchVolumes_.begin(),
        launchVolumes_.end(),
        [&position](const LaunchVolume& volume) {
            return IsPointInsideVolume(position, volume);
        });
    return it == launchVolumes_.end() ? nullptr : &(*it);
}

std::vector<const AnalyticsHeatmapVolume*> RuntimeEnvironmentService::GetAnalyticsHeatmapVolumesAt(
    const ri::math::Vec3& position) const {
    std::vector<const AnalyticsHeatmapVolume*> matches;
    for (const AnalyticsHeatmapVolume& volume : analyticsHeatmapVolumes_) {
        if (IsPointInsideVolume(position, volume)) {
            matches.push_back(&volume);
        }
    }
    return matches;
}

std::size_t RuntimeEnvironmentService::GetTriggerIndexEntryCount() const {
    RebuildTriggerIndexIfNeeded();
    return triggerIndex_.EntryCount();
}

bool RuntimeEnvironmentService::ApplyGenericTriggerVolumeLogicInput(std::string_view volumeId,
                                                                    std::string_view inputName) {
    const std::string in = NormalizeAsciiLower(inputName);
    for (GenericTriggerVolume& volume : genericTriggerVolumes_) {
        if (volume.id != volumeId) {
            continue;
        }
        if (in == "enable" || in == "turnon") {
            volume.armed = true;
        } else if (in == "disable" || in == "turnoff") {
            volume.armed = false;
        } else if (in == "toggle") {
            volume.armed = !volume.armed;
        } else {
            return true;
        }
        return true;
    }
    return false;
}

void RuntimeEnvironmentService::RegisterLogicDoor(std::string id, LogicDoorRuntimeState initial) {
    worldActorKindById_[id] = "door";
    logicDoorActors_[std::move(id)] = std::move(initial);
}

void RuntimeEnvironmentService::RegisterLogicSpawner(std::string id, LogicSpawnerRuntimeState initial) {
    worldActorKindById_[id] = "spawner";
    logicSpawnerActors_[std::move(id)] = std::move(initial);
}

void RuntimeEnvironmentService::ClearLogicWorldActors() {
    for (const auto& entry : logicDoorActors_) {
        worldActorKindById_.erase(entry.first);
    }
    for (const auto& entry : logicSpawnerActors_) {
        worldActorKindById_.erase(entry.first);
    }
    logicDoorActors_.clear();
    logicSpawnerActors_.clear();
}

void RuntimeEnvironmentService::SetLevelSpawnerDefinitions(std::vector<LevelSpawnerDefinition> definitions) {
    for (const LevelSpawnerDefinition& existing : levelSpawnerDefinitions_) {
        worldActorKindById_.erase(existing.id);
    }
    levelSpawnerDefinitions_ = std::move(definitions);
    logicSpawnerActors_.clear();
    for (const LevelSpawnerDefinition& definition : levelSpawnerDefinitions_) {
        worldActorKindById_[definition.id] = "spawner";
        logicSpawnerActors_[definition.id] = LogicSpawnerRuntimeState{
            .activeSpawn = false,
            .enabled = definition.enabledByDefault,
        };
    }
}

const std::vector<LevelSpawnerDefinition>& RuntimeEnvironmentService::GetLevelSpawnerDefinitions() const {
    return levelSpawnerDefinitions_;
}

std::vector<ActiveSpawnerState> RuntimeEnvironmentService::GetActiveSpawnerStates() const {
    std::vector<ActiveSpawnerState> states{};
    states.reserve(logicSpawnerActors_.size());
    for (const auto& [id, runtimeState] : logicSpawnerActors_) {
        ActiveSpawnerState state{};
        state.id = id;
        state.activeSpawn = runtimeState.activeSpawn;
        state.enabled = runtimeState.enabled;
        const auto definition = std::find_if(
            levelSpawnerDefinitions_.begin(),
            levelSpawnerDefinitions_.end(),
            [&id](const LevelSpawnerDefinition& entry) { return entry.id == id; });
        if (definition != levelSpawnerDefinitions_.end()) {
            state.entityId = definition->entityId;
        }
        states.push_back(std::move(state));
    }
    return states;
}

void RuntimeEnvironmentService::SetLevelActionGroups(std::vector<LevelActionGroup> groups) {
    levelActionGroups_ = std::move(groups);
}

void RuntimeEnvironmentService::SetLevelTargetGroups(std::vector<LevelTargetGroup> groups) {
    levelTargetGroups_ = std::move(groups);
}

void RuntimeEnvironmentService::SetLevelEvents(std::vector<LevelEvent> events) {
    levelEvents_ = std::move(events);
}

void RuntimeEnvironmentService::SetLevelSequences(std::vector<LevelSequence> sequences) {
    levelSequences_ = std::move(sequences);
}

bool RuntimeEnvironmentService::DispatchLevelSequenceStep(ri::logic::LogicGraph& graph,
                                                          std::string_view sequenceId,
                                                          std::size_t actionGroupIndex,
                                                          const ri::logic::LogicContext& context) {
    const auto sequence = std::find_if(
        levelSequences_.begin(),
        levelSequences_.end(),
        [sequenceId](const LevelSequence& candidate) { return candidate.id == sequenceId; });
    if (sequence == levelSequences_.end() || sequence->actionGroupIds.empty()) {
        return false;
    }

    const std::size_t resolvedIndex = sequence->loop
        ? (actionGroupIndex % sequence->actionGroupIds.size())
        : std::min(actionGroupIndex, sequence->actionGroupIds.size() - 1U);
    const std::string& actionGroupId = sequence->actionGroupIds[resolvedIndex];

    auto actionGroupIt = std::find_if(
        levelActionGroups_.begin(),
        levelActionGroups_.end(),
        [&actionGroupId](const LevelActionGroup& candidate) { return candidate.id == actionGroupId; });
    if (actionGroupIt == levelActionGroups_.end()) {
        return false;
    }

    bool anyApplied = false;
    for (const LevelAction& action : actionGroupIt->actions) {
        std::vector<std::string> targets{};
        targets.push_back(action.targetId);
        const auto targetGroup = std::find_if(
            levelTargetGroups_.begin(),
            levelTargetGroups_.end(),
            [&action](const LevelTargetGroup& group) { return group.id == action.targetId; });
        if (targetGroup != levelTargetGroups_.end()) {
            targets = targetGroup->targetIds;
        }
        for (const std::string& targetId : targets) {
            anyApplied = ApplyWorldActorLogicInput(graph, targetId, action.inputName, context) || anyApplied;
        }
    }
    return anyApplied;
}

bool RuntimeEnvironmentService::DispatchLevelEvent(ri::logic::LogicGraph& graph,
                                                   std::string_view eventId,
                                                   const ri::logic::LogicContext& context) {
    const auto eventIt = std::find_if(
        levelEvents_.begin(),
        levelEvents_.end(),
        [eventId](const LevelEvent& event) { return event.id == eventId; });
    if (eventIt == levelEvents_.end()) {
        return false;
    }

    bool anyApplied = false;
    for (const std::string& actionGroupId : eventIt->actionGroupIds) {
        const auto sequence = std::find_if(
            levelSequences_.begin(),
            levelSequences_.end(),
            [&actionGroupId](const LevelSequence& value) { return value.id == actionGroupId; });
        if (sequence != levelSequences_.end()) {
            anyApplied = DispatchLevelSequenceStep(graph, sequence->id, 0U, context) || anyApplied;
            continue;
        }
        const auto group = std::find_if(
            levelActionGroups_.begin(),
            levelActionGroups_.end(),
            [&actionGroupId](const LevelActionGroup& value) { return value.id == actionGroupId; });
        if (group == levelActionGroups_.end()) {
            continue;
        }
        for (const LevelAction& action : group->actions) {
            anyApplied = ApplyWorldActorLogicInput(graph, action.targetId, action.inputName, context) || anyApplied;
        }
    }
    return anyApplied;
}

void RuntimeEnvironmentService::SetSafeLights(std::vector<SafeLightRuntimeState> lights) {
    safeLights_ = std::move(lights);
}

const std::vector<SafeLightRuntimeState>& RuntimeEnvironmentService::GetSafeLights() const {
    return safeLights_;
}

SafeLightCoverageState RuntimeEnvironmentService::GetSafeLightCoverageAt(const ri::math::Vec3& position) const {
    SafeLightCoverageState state{};
    for (const SafeLightRuntimeState& light : safeLights_) {
        if (!light.enabled || !light.safeZone || light.radius <= 0.01f) {
            continue;
        }
        const float distance = ri::math::Length(position - light.position);
        if (distance > light.radius) {
            continue;
        }
        const float coverage = std::clamp(1.0f - (distance / light.radius), 0.0f, 1.0f) * std::max(0.0f, light.intensity);
        state.strongestCoverage = std::max(state.strongestCoverage, coverage);
        state.combinedCoverage += coverage;
        state.activeSafeLightIds.push_back(light.id);
    }
    state.combinedCoverage = std::clamp(state.combinedCoverage, 0.0f, 1.0f);
    state.insideSafeLight = !state.activeSafeLightIds.empty();
    return state;
}

bool RuntimeEnvironmentService::SetLevelLightEnabled(std::string_view lightId, bool enabled) {
    auto light = std::find_if(
        safeLights_.begin(),
        safeLights_.end(),
        [lightId](const SafeLightRuntimeState& value) { return value.id == lightId; });
    if (light == safeLights_.end()) {
        return false;
    }
    light->enabled = enabled;
    return true;
}

bool RuntimeEnvironmentService::SetLevelLightIntensity(std::string_view lightId, const float intensity) {
    auto light = std::find_if(
        safeLights_.begin(),
        safeLights_.end(),
        [lightId](const SafeLightRuntimeState& value) { return value.id == lightId; });
    if (light == safeLights_.end()) {
        return false;
    }
    light->intensity = intensity;
    return true;
}

void RuntimeEnvironmentService::SetEntityInputPipeline(std::unique_ptr<EntityInputDispatchPipeline> pipeline) {
    entityInputPipeline_ = std::move(pipeline);
}

void RuntimeEnvironmentService::RegisterWorldActorKind(std::string actorId, std::string kind) {
    worldActorKindById_[std::move(actorId)] = std::move(kind);
}

std::optional<std::string> RuntimeEnvironmentService::ResolveWorldActorKind(const std::string_view actorId) const {
    const auto found = worldActorKindById_.find(std::string(actorId));
    if (found == worldActorKindById_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::size_t RuntimeEnvironmentService::SetLevelLightGroupEnabled(std::string_view groupId, bool enabled) {
    std::size_t changed = 0;
    for (SafeLightRuntimeState& light : safeLights_) {
        if (light.groupId != groupId) {
            continue;
        }
        light.enabled = enabled;
        ++changed;
    }
    return changed;
}

void RuntimeEnvironmentService::SetWorldFlag(std::string key, bool enabled) {
    if (key.empty()) {
        return;
    }
    if (enabled) {
        worldFlags_.insert(std::move(key));
    } else {
        worldFlags_.erase(key);
    }
}

void RuntimeEnvironmentService::ClearWorldFlag(std::string_view key) {
    if (key.empty()) {
        return;
    }
    worldFlags_.erase(std::string(key));
}

bool RuntimeEnvironmentService::HasWorldFlag(std::string_view key) const {
    if (key.empty()) {
        return false;
    }
    return worldFlags_.contains(std::string(key));
}

void RuntimeEnvironmentService::SetWorldValue(std::string key, double value) {
    if (key.empty() || !std::isfinite(value)) {
        return;
    }
    worldValues_[std::move(key)] = value;
}

double RuntimeEnvironmentService::GetWorldValueOr(std::string_view key, double fallback) const {
    if (key.empty()) {
        return fallback;
    }
    const auto it = worldValues_.find(std::string(key));
    if (it == worldValues_.end() || !std::isfinite(it->second)) {
        return fallback;
    }
    return it->second;
}

bool RuntimeEnvironmentService::CheckWorldStateCondition(const WorldStateCondition& condition) const {
    for (const std::string& flag : condition.requiredFlags) {
        if (!HasWorldFlag(flag)) {
            return false;
        }
    }
    for (const std::string& flag : condition.forbiddenFlags) {
        if (HasWorldFlag(flag)) {
            return false;
        }
    }
    for (const auto& [key, threshold] : condition.minimumValues) {
        if (GetWorldValueOr(key, std::numeric_limits<double>::lowest()) < threshold) {
            return false;
        }
    }
    for (const auto& [key, threshold] : condition.maximumValues) {
        if (GetWorldValueOr(key, std::numeric_limits<double>::max()) > threshold) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> RuntimeEnvironmentService::GetRegisteredLogicDoorIds() const {
    std::vector<std::string> ids{};
    ids.reserve(logicDoorActors_.size());
    for (const auto& entry : logicDoorActors_) {
        ids.push_back(entry.first);
    }
    return ids;
}

std::vector<std::string> RuntimeEnvironmentService::GetRegisteredLogicSpawnerIds() const {
    std::vector<std::string> ids{};
    ids.reserve(logicSpawnerActors_.size());
    for (const auto& entry : logicSpawnerActors_) {
        ids.push_back(entry.first);
    }
    return ids;
}

bool RuntimeEnvironmentService::ApplyWorldActorLogicInput(ri::logic::LogicGraph& graph,
                                                          std::string_view actorId,
                                                          std::string_view inputName,
                                                          const ri::logic::LogicContext& context) {
    if (entityInputPipeline_) {
        if (const std::optional<std::string> kind = ResolveWorldActorKind(actorId); kind.has_value()) {
            if (entityInputPipeline_->TryDispatch(*this, graph, *kind, actorId, inputName, context)) {
                return true;
            }
        }
    }
    if (ApplyGenericTriggerVolumeLogicInput(actorId, inputName)) {
        return true;
    }
    const std::string id(actorId);
    const std::string in = NormalizeAsciiLower(inputName);
    if (auto it = logicDoorActors_.find(id); it != logicDoorActors_.end()) {
        return ApplyLogicDoorActorInput(graph, id, it->second, in, context);
    }
    if (auto it = logicSpawnerActors_.find(id); it != logicSpawnerActors_.end()) {
        return ApplyLogicSpawnerActorInput(graph, id, it->second, in, context);
    }
    auto light = std::find_if(
        safeLights_.begin(), safeLights_.end(), [&id](const SafeLightRuntimeState& value) {
            return value.id == id;
        });
    if (light != safeLights_.end()) {
        if (in == "enable" || in == "turnon") {
            light->enabled = true;
        } else if (in == "disable" || in == "turnoff") {
            light->enabled = false;
        } else if (in == "toggle") {
            light->enabled = !light->enabled;
        } else {
            return false;
        }
        return true;
    }
    constexpr std::string_view kLightGroupPrefix = "light_group:";
    if (id.rfind(kLightGroupPrefix, 0U) == 0U) {
        const std::string_view group = std::string_view(id).substr(kLightGroupPrefix.size());
        if (in == "toggle") {
            bool anyEnabled = false;
            for (const SafeLightRuntimeState& safeLight : safeLights_) {
                if (safeLight.groupId == group && safeLight.enabled) {
                    anyEnabled = true;
                    break;
                }
            }
            SetLevelLightGroupEnabled(group, !anyEnabled);
            return true;
        }
        if (in == "enable" || in == "turnon" || in == "disable" || in == "turnoff") {
            const bool enabled = (in == "enable" || in == "turnon");
            return SetLevelLightGroupEnabled(group, enabled) > 0U;
        }
    }
    return false;
}

EnvironmentalVolumeUpdateResult RuntimeEnvironmentService::UpdateEnvironmentalVolumesAt(const ri::math::Vec3& position) {
    EnvironmentalVolumeUpdateResult result{};
    auto evaluate = [&](const RuntimeVolume& volume) {
        const bool inside = IsPointInsideVolume(position, volume);
        const bool wasInside = environmentalInsideById_[volume.id];
        if (inside == wasInside) {
            return;
        }
        environmentalInsideById_[volume.id] = inside;
        result.transitions.push_back(EnvironmentalVolumeTransition{
            .volumeId = volume.id,
            .volumeType = volume.type,
            .kind = inside ? EnvironmentalTransitionKind::Enter : EnvironmentalTransitionKind::Exit,
        });
    };

    for (const PostProcessVolume& volume : postProcessVolumes_) {
        evaluate(volume);
    }
    for (const LocalizedFogVolume& volume : localizedFogVolumes_) {
        evaluate(volume);
    }
    for (const AudioReverbVolume& volume : audioReverbVolumes_) {
        evaluate(volume);
    }
    for (const AudioOcclusionVolume& volume : audioOcclusionVolumes_) {
        evaluate(volume);
    }
    for (const FluidSimulationVolume& volume : fluidSimulationVolumes_) {
        evaluate(volume);
    }
    std::sort(result.transitions.begin(), result.transitions.end(), [](const EnvironmentalVolumeTransition& lhs,
                                                                       const EnvironmentalVolumeTransition& rhs) {
        if (lhs.volumeId != rhs.volumeId) {
            return lhs.volumeId < rhs.volumeId;
        }
        return static_cast<int>(lhs.kind) < static_cast<int>(rhs.kind);
    });
    return result;
}

TriggerUpdateResult RuntimeEnvironmentService::UpdateTriggerVolumesAt(
    const ri::math::Vec3& position,
    double elapsedSeconds,
    ri::runtime::RuntimeEventBus* eventBus,
    bool simulationTimeAdvances,
    ri::logic::LogicGraph* logicGraph,
    const ri::logic::LogicContext* logicTriggerContext) {
    TriggerUpdateResult result{};
    const std::vector<std::string> candidateKeysVector = QueryTriggerVolumeKeysAt(position);
    const std::unordered_set<std::string> candidateKeys(candidateKeysVector.begin(), candidateKeysVector.end());

    UpdateTriggerFamily(
        damageVolumes_,
        position,
        elapsedSeconds,
        candidateKeys,
        result,
        eventBus,
        [&result](DamageTriggerVolume& volume) {
            result.damageRequests.push_back(DamageRequest{
                .volumeId = volume.id,
                .damagePerSecond = std::max(0.0f, volume.damagePerSecond),
                .killInstant = volume.killInstant,
                .label = volume.label,
            });
        },
        [&result](DamageTriggerVolume& volume) {
            result.damageRequests.push_back(DamageRequest{
                .volumeId = volume.id,
                .damagePerSecond = std::max(0.0f, volume.damagePerSecond),
                .killInstant = volume.killInstant,
                .label = volume.label,
            });
        },
        [](DamageTriggerVolume&) {});

    UpdateTriggerFamily(
        genericTriggerVolumes_,
        position,
        elapsedSeconds,
        candidateKeys,
        result,
        eventBus,
        [](GenericTriggerVolume&) {},
        [](GenericTriggerVolume&) {},
        [](GenericTriggerVolume&) {});

    UpdateTriggerFamily(
        spatialQueryVolumes_,
        position,
        elapsedSeconds,
        candidateKeys,
        result,
        eventBus,
        [](SpatialQueryVolume&) {},
        [](SpatialQueryVolume&) {},
        [](SpatialQueryVolume&) {});

    UpdateTriggerFamily(
        streamingLevelVolumes_,
        position,
        elapsedSeconds,
        candidateKeys,
        result,
        eventBus,
        [&result](StreamingLevelVolume& volume) {
            if (!volume.targetLevel.empty()) {
                result.streamingRequests.push_back(StreamingLevelRequest{
                    .volumeId = volume.id,
                    .targetLevel = volume.targetLevel,
                });
            }
        },
        [](StreamingLevelVolume&) {},
        [](StreamingLevelVolume&) {});

    UpdateTriggerFamily(
        checkpointSpawnVolumes_,
        position,
        elapsedSeconds,
        candidateKeys,
        result,
        eventBus,
        [&result](CheckpointSpawnVolume& volume) {
            result.checkpointRequests.push_back(CheckpointSpawnRequest{
                .volumeId = volume.id,
                .targetLevel = volume.targetLevel,
                .respawn = volume.respawn,
                .respawnRotation = volume.respawnRotation,
            });
        },
        [](CheckpointSpawnVolume&) {},
        [](CheckpointSpawnVolume&) {});

    for (const CheckpointSpawnRequest& request : result.checkpointRequests) {
        ArmSpawnStabilization(request.respawn, elapsedSeconds, 0.25);
        ArmCameraShake(elapsedSeconds, 0.25f, {0.035f, 0.025f, 0.0f}, 0.22);
    }

    UpdateTriggerFamily(
        teleportVolumes_,
        position,
        elapsedSeconds,
        candidateKeys,
        result,
        eventBus,
        [&result](TeleportVolume& volume) {
            result.teleportRequests.push_back(TeleportRequest{
                .volumeId = volume.id,
                .targetId = volume.targetId,
                .targetPosition = volume.targetPosition,
                .targetRotation = volume.targetRotation,
                .offset = volume.offset,
            });
        },
        [](TeleportVolume&) {},
        [](TeleportVolume&) {});

    UpdateTriggerFamily(
        launchVolumes_,
        position,
        elapsedSeconds,
        candidateKeys,
        result,
        eventBus,
        [&result](LaunchVolume& volume) {
            result.launchRequests.push_back(LaunchRequest{
                .volumeId = volume.id,
                .impulse = volume.impulse,
                .affectPhysics = volume.affectPhysics,
            });
        },
        [](LaunchVolume&) {},
        [](LaunchVolume&) {});

    UpdateTriggerFamily(
        analyticsHeatmapVolumes_,
        position,
        elapsedSeconds,
        candidateKeys,
        result,
        eventBus,
        [&result](AnalyticsHeatmapVolume& volume) {
            if ((volume.sampleSubjectMask & kAnalyticsHeatmapSamplePlayer) == 0U) {
                return;
            }
            volume.entryCount += 1U;
            result.analyticsEnteredVolumes.push_back(volume.id);
        },
        [](AnalyticsHeatmapVolume&) {},
        [](AnalyticsHeatmapVolume&) {});

    if (std::isfinite(elapsedSeconds)) {
        if (lastTriggerUpdateSeconds_ >= 0.0 && std::isfinite(lastTriggerUpdateSeconds_)
            && elapsedSeconds >= lastTriggerUpdateSeconds_) {
            const double dt = elapsedSeconds - lastTriggerUpdateSeconds_;
            if (simulationTimeAdvances && dt > 0.0) {
                AccumulateAnalyticsHeatmapTimeAt(
                    position, dt, true, kAnalyticsHeatmapSamplePlayer);
            }
        }
        lastTriggerUpdateSeconds_ = elapsedSeconds;
    }

    if (logicGraph != nullptr && logicTriggerContext != nullptr) {
        ApplyTriggerTransitionsToLogicGraph(*logicGraph, result, *logicTriggerContext);
    }

    for (const TriggerTransition& transition : result.transitions) {
        const TriggerRuntimeVolume* sourceVolume = nullptr;
        auto findVolume = [&](const auto& collection) {
            const auto found = std::find_if(collection.begin(), collection.end(), [&](const auto& value) {
                return value.id == transition.volumeId;
            });
            if (found != collection.end()) {
                sourceVolume = &(*found);
            }
        };
        findVolume(genericTriggerVolumes_);
        if (sourceVolume == nullptr) {
            findVolume(spatialQueryVolumes_);
        }
        if (sourceVolume == nullptr) {
            findVolume(streamingLevelVolumes_);
        }
        if (sourceVolume == nullptr) {
            findVolume(checkpointSpawnVolumes_);
        }
        if (sourceVolume == nullptr) {
            findVolume(teleportVolumes_);
        }
        if (sourceVolume == nullptr) {
            findVolume(launchVolumes_);
        }
        if (sourceVolume == nullptr) {
            findVolume(damageVolumes_);
        }
        if (sourceVolume == nullptr) {
            findVolume(analyticsHeatmapVolumes_);
        }
        if (sourceVolume == nullptr) {
            continue;
        }

        std::string_view eventId;
        switch (transition.kind) {
            case TriggerTransitionKind::Enter:
                eventId = sourceVolume->onEnterEvent;
                break;
            case TriggerTransitionKind::Stay:
                eventId = sourceVolume->onStayEvent;
                break;
            case TriggerTransitionKind::Exit:
                eventId = sourceVolume->onExitEvent;
                break;
        }
        if (eventId.empty()) {
            continue;
        }

        if (logicGraph != nullptr && logicTriggerContext != nullptr) {
            [[maybe_unused]] const bool dispatched = DispatchLevelEvent(*logicGraph, eventId, *logicTriggerContext);
        }
        if (eventBus != nullptr) {
            EmitGameplayFlowEvent(eventBus, "triggerNamedEvent", transition.volumeId, std::string(eventId));
        }
    }

    if (eventBus != nullptr) {
        for (const StreamingLevelRequest& request : result.streamingRequests) {
            EmitGameplayFlowEvent(eventBus, "streamingLevelRequested", request.volumeId, request.targetLevel);
        }
        for (const CheckpointSpawnRequest& request : result.checkpointRequests) {
            EmitGameplayFlowEvent(eventBus, "checkpointRequested", request.volumeId, request.targetLevel);
        }
        for (const TeleportRequest& request : result.teleportRequests) {
            EmitGameplayFlowEvent(eventBus, "teleportRequested", request.volumeId, request.targetId);
        }
        for (const LaunchRequest& request : result.launchRequests) {
            EmitGameplayFlowEvent(eventBus, "launchRequested", request.volumeId, request.affectPhysics ? "physics" : "kinematic");
        }
        for (const DamageRequest& request : result.damageRequests) {
            EmitGameplayFlowEvent(eventBus, "damageRequested", request.volumeId, request.label);
        }
    }

    return result;
}

void RuntimeEnvironmentService::MarkTriggerIndexDirty() {
    triggerIndexDirty_ = true;
}

void RuntimeEnvironmentService::RebuildTriggerIndexIfNeeded() const {
    if (!triggerIndexDirty_) {
        return;
    }

    const auto startedAt = std::chrono::steady_clock::now();
    std::vector<ri::spatial::SpatialEntry> entries;
    const auto reserveCount = genericTriggerVolumes_.size()
        + damageVolumes_.size()
        + spatialQueryVolumes_.size()
        + streamingLevelVolumes_.size()
        + checkpointSpawnVolumes_.size()
        + teleportVolumes_.size()
        + launchVolumes_.size()
        + analyticsHeatmapVolumes_.size();
    entries.reserve(reserveCount);

    const auto appendEntries = [&entries](const auto& volumes) {
        for (const auto& volume : volumes) {
            entries.push_back(ri::spatial::SpatialEntry{
                .id = BuildTriggerIndexKey(volume.type, volume.id),
                .bounds = BuildRuntimeVolumeBounds(volume),
            });
        }
    };

    appendEntries(genericTriggerVolumes_);
    appendEntries(damageVolumes_);
    appendEntries(spatialQueryVolumes_);
    appendEntries(streamingLevelVolumes_);
    appendEntries(checkpointSpawnVolumes_);
    appendEntries(teleportVolumes_);
    appendEntries(launchVolumes_);
    appendEntries(analyticsHeatmapVolumes_);

    triggerIndex_.Rebuild(std::move(entries));
    triggerIndexDirty_ = false;

    if (spatialQueryTracker_ != nullptr) {
        const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - startedAt;
        spatialQueryTracker_->RecordTriggerIndexBuild(elapsed.count());
    }
}

std::vector<std::string> RuntimeEnvironmentService::QueryTriggerVolumeKeysAt(const ri::math::Vec3& position) const {
    RebuildTriggerIndexIfNeeded();
    const ri::spatial::SpatialIndexMetrics beforeMetrics = triggerIndex_.Metrics();
    if (triggerIndex_.Empty()) {
        if (spatialQueryTracker_ != nullptr) {
            spatialQueryTracker_->RecordTriggerPointQuery(0U, 0U);
        }
        return {};
    }

    constexpr float kTriggerQueryPadding = 0.05f;
    const ri::math::Vec3 padding{kTriggerQueryPadding, kTriggerQueryPadding, kTriggerQueryPadding};
    const ri::spatial::Aabb queryBounds{
        .min = position - padding,
        .max = position + padding,
    };
    std::vector<std::string> matches = triggerIndex_.QueryBox(queryBounds);
    if (spatialQueryTracker_ != nullptr) {
        const ri::spatial::SpatialIndexMetrics afterMetrics = triggerIndex_.Metrics();
        const std::size_t scanned = afterMetrics.boxCandidatesScanned - beforeMetrics.boxCandidatesScanned;
        spatialQueryTracker_->RecordTriggerPointQuery(matches.size(), scanned);
    }
    return matches;
}

std::size_t RuntimeEnvironmentService::MarkAnalyticsHeatmapEntryAt(const ri::math::Vec3& position,
                                                                   std::uint32_t subjectMask) {
    std::size_t touched = 0;
    for (AnalyticsHeatmapVolume& volume : analyticsHeatmapVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        if ((volume.sampleSubjectMask & subjectMask) == 0U) {
            continue;
        }
        volume.entryCount += 1;
        touched += 1;
    }
    return touched;
}

void RuntimeEnvironmentService::AccumulateAnalyticsHeatmapTimeAt(const ri::math::Vec3& position,
                                                                 double deltaSeconds,
                                                                 bool simulationTimeAdvances,
                                                                 std::uint32_t subjectMask) {
    if (!simulationTimeAdvances || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) {
        return;
    }

    for (AnalyticsHeatmapVolume& volume : analyticsHeatmapVolumes_) {
        if (!IsPointInsideVolume(position, volume)) {
            continue;
        }
        if ((volume.sampleSubjectMask & subjectMask) == 0U) {
            continue;
        }
        volume.dwellSeconds += deltaSeconds;
    }
}

void RuntimeEnvironmentService::ResetAnalyticsHeatmapStatistics() {
    for (AnalyticsHeatmapVolume& volume : analyticsHeatmapVolumes_) {
        volume.entryCount = 0U;
        volume.dwellSeconds = 0.0;
        volume.playerInside = false;
        volume.nextBroadcastAt = 0.0;
    }
    lastTriggerUpdateSeconds_ = -1.0;
}

RuntimeEnvironmentState RuntimeEnvironmentService::GetActiveEnvironmentStateAt(const ri::math::Vec3& position,
                                                                               double timeSeconds) const {
    RuntimeEnvironmentState state{};
    state.postProcess = GetActivePostProcessStateAt(position);
    state.audio = GetActiveAudioEnvironmentStateAt(position);
    state.physics = GetPhysicsVolumeModifiersAt(position);
    state.constraints = GetPhysicsConstraintStateAt(position);
    state.waterSurface = GetWaterSurfaceStateAt(position, timeSeconds);
    state.kinematicMotion = ResolveKinematicMotionAt(position, timeSeconds);
    return state;
}

void RuntimeEnvironmentService::ArmCameraShake(const double nowSeconds,
                                               const float amount,
                                               const ri::math::Vec3& offset,
                                               double durationSeconds) {
    durationSeconds = std::isfinite(durationSeconds) ? std::max(0.0, durationSeconds) : 0.0;
    const double nextShakeUntil = (std::isfinite(nowSeconds) ? nowSeconds : 0.0) + durationSeconds;
    shakeUntil_ = std::max(shakeUntil_, nextShakeUntil);
    shakeAmount_ = ClampFloatWithFallback(amount, 0.0f, 0.0f, 1.0f);
    shakeOffset_.x = ClampFloatWithFallback(offset.x, 0.0f, -0.25f, 0.25f);
    shakeOffset_.y = ClampFloatWithFallback(offset.y, 0.0f, -0.25f, 0.25f);
    shakeOffset_.z = ClampFloatWithFallback(offset.z, 0.0f, -0.25f, 0.25f);
}

void RuntimeEnvironmentService::UpdatePresentationFeedback(const double nowSeconds, double /*deltaSeconds*/) {
    if (!std::isfinite(nowSeconds)) {
        return;
    }
    if (nowSeconds >= shakeUntil_) {
        shakeAmount_ = 0.0f;
        shakeOffset_ = {};
    }
}

CameraShakePresentationState RuntimeEnvironmentService::GetCameraShakePresentationState(const double nowSeconds) const {
    CameraShakePresentationState state{};
    state.active = std::isfinite(nowSeconds) && nowSeconds < shakeUntil_ && shakeAmount_ > 0.0f;
    state.shakeUntil = shakeUntil_;
    state.shakeAmount = shakeAmount_;
    state.shakeOffset = shakeOffset_;
    return state;
}

void RuntimeEnvironmentService::ArmSpawnStabilization(const ri::math::Vec3& anchor,
                                                      const double nowSeconds,
                                                      double settleSeconds) {
    settleSeconds = std::isfinite(settleSeconds) ? std::max(0.0, settleSeconds) : 0.0;
    pendingSpawnStabilization_ = true;
    spawnStabilizationAnchor_ = anchor;
    spawnStabilizeUntil_ = (std::isfinite(nowSeconds) ? nowSeconds : 0.0) + settleSeconds;
}

bool RuntimeEnvironmentService::StabilizeFreshSpawnIfNeeded(const double nowSeconds,
                                                            ri::math::Vec3& inOutPosition,
                                                            ri::math::Vec3* inOutVelocity) {
    if (!pendingSpawnStabilization_) {
        return false;
    }
    if (std::isfinite(nowSeconds) && nowSeconds > spawnStabilizeUntil_) {
        pendingSpawnStabilization_ = false;
        return false;
    }
    inOutPosition = spawnStabilizationAnchor_;
    if (inOutVelocity != nullptr) {
        *inOutVelocity = {};
    }
    pendingSpawnStabilization_ = false;
    return true;
}

SpawnStabilizationState RuntimeEnvironmentService::GetSpawnStabilizationState() const {
    SpawnStabilizationState state{};
    state.pendingSpawnStabilization = pendingSpawnStabilization_;
    state.anchor = spawnStabilizationAnchor_;
    state.settleUntil = spawnStabilizeUntil_;
    return state;
}

bool IsPointInsideVolume(const ri::math::Vec3& point, const RuntimeVolume& volume) {
    if (volume.shape == VolumeShape::Box) {
        return std::fabs(point.x - volume.position.x) <= volume.size.x * 0.5f
            && std::fabs(point.y - volume.position.y) <= volume.size.y * 0.5f
            && std::fabs(point.z - volume.position.z) <= volume.size.z * 0.5f;
    }
    if (volume.shape == VolumeShape::Cylinder) {
        const float radius = std::max(0.25f, std::isfinite(volume.radius) ? volume.radius : 0.5f);
        const float height = std::max(0.25f, std::isfinite(volume.height) ? volume.height : volume.size.y);
        const float dx = point.x - volume.position.x;
        const float dz = point.z - volume.position.z;
        return ((dx * dx) + (dz * dz)) <= (radius * radius)
            && std::fabs(point.y - volume.position.y) <= height * 0.5f;
    }

    const float radius = std::max(0.0f, std::isfinite(volume.radius) ? volume.radius : 0.0f);
    return ri::math::Distance(point, volume.position) <= radius;
}

std::vector<AnalyticsHeatmapExportRow> BuildAnalyticsHeatmapExportRows(
    const RuntimeEnvironmentService& environment,
    std::string_view levelId,
    std::string_view sessionId,
    std::string_view buildId) {
    std::vector<AnalyticsHeatmapExportRow> rows;
    const std::vector<AnalyticsHeatmapVolume>& volumes = environment.GetAnalyticsHeatmapVolumes();
    rows.reserve(volumes.size());
    for (const AnalyticsHeatmapVolume& volume : volumes) {
        AnalyticsHeatmapExportRow row{};
        row.levelId = std::string(levelId);
        row.volumeId = volume.id;
        row.entryCount = static_cast<std::uint64_t>(volume.entryCount);
        row.dwellSeconds = volume.dwellSeconds;
        row.sessionId = std::string(sessionId);
        row.buildId = std::string(buildId);
        rows.push_back(std::move(row));
    }
    return rows;
}

RuntimeHelperMetricsSnapshot BuildRuntimeHelperMetricsSnapshot(
    std::string_view runtimeSessionId,
    const ri::runtime::RuntimeEventBusMetrics& eventBusMetrics,
    const ri::validation::SchemaValidationMetrics& schemaMetrics,
    const ri::audio::AudioManagerMetrics& audioMetrics,
    const RuntimeStatsOverlayMetrics& statsMetrics,
    const HelperActivityState& activity,
    const RuntimeEnvironmentService& environmentService,
    const std::optional<ri::structural::StructuralGraphSummary>& structuralSummary,
    const PostProcessState& activePostProcessState,
    const std::optional<ri::structural::StructuralDeferredPipelineResult>& deferredPipelineResult) {
    RuntimeHelperMetricsSnapshot snapshot{};
    snapshot.schemaValidations = schemaMetrics.levelValidations;
    snapshot.schemaValidationFailures = schemaMetrics.levelValidationFailures;
    snapshot.tuningParses = schemaMetrics.tuningParses;
    snapshot.tuningParseFailures = schemaMetrics.tuningParseFailures;
    snapshot.eventBusEmits = eventBusMetrics.emitted;
    snapshot.eventBusListeners = eventBusMetrics.activeListeners;
    snapshot.eventBusListenersAdded = eventBusMetrics.listenersAdded;
    snapshot.eventBusListenersRemoved = eventBusMetrics.listenersRemoved;
    snapshot.audioManagedSounds = audioMetrics.managedSounds;
    snapshot.audioLoopsCreated = audioMetrics.loopsCreated;
    snapshot.audioOneShotsPlayed = audioMetrics.oneShotsPlayed;
    snapshot.audioVoicesPlayed = audioMetrics.voicesPlayed;
    snapshot.audioVoiceActive = audioMetrics.voiceActive;
    snapshot.audioEnvironmentChanges = audioMetrics.environmentChanges;
    snapshot.audioEnvironment = audioMetrics.activeEnvironment;
    snapshot.audioEnvironmentMix = audioMetrics.activeEnvironmentMix;

    if (!runtimeSessionId.empty()) {
        std::string label(runtimeSessionId);
        if (label.rfind("session_", 0) == 0) {
            label.replace(0, 8, "session:");
        }
        snapshot.runtimeSession = SummarizeHelperActivity(label, 28);
    }

    snapshot.lastAudioEvent = SummarizeHelperActivity(activity.lastAudioEvent, 24);
    snapshot.lastStateChange = SummarizeHelperActivity(activity.lastStateChange, 24);
    snapshot.lastTriggerEvent = SummarizeHelperActivity(activity.lastTriggerEvent, 24);
    snapshot.lastEntityIoEvent = SummarizeHelperActivity(activity.lastEntityIoEvent, 24);
    snapshot.lastMessage = SummarizeHelperActivity(activity.lastMessage, 30);
    snapshot.lastLevelEvent = SummarizeHelperActivity(activity.lastLevelEvent, 30);
    snapshot.lastSchemaEvent = SummarizeHelperActivity(activity.lastSchemaEvent, 30);

    snapshot.postProcessVolumes = environmentService.GetPostProcessVolumes().size();
    snapshot.audioReverbVolumes = environmentService.GetAudioReverbVolumes().size();
    snapshot.audioOcclusionVolumes = environmentService.GetAudioOcclusionVolumes().size();
    snapshot.ambientAudioVolumes = environmentService.GetAmbientAudioVolumes().size();
    snapshot.localizedFogVolumes = environmentService.GetLocalizedFogVolumes().size();
    snapshot.volumetricFogBlockers = environmentService.GetFogBlockerVolumes().size();
    snapshot.fluidSimulationVolumes = environmentService.GetFluidSimulationVolumes().size();
    snapshot.physicsModifierVolumes = environmentService.GetPhysicsModifierVolumes().size();
    snapshot.surfaceVelocityPrimitives = environmentService.GetSurfaceVelocityPrimitives().size();
    snapshot.waterSurfacePrimitives = environmentService.GetWaterSurfacePrimitives().size();
    snapshot.radialForceVolumes = environmentService.GetRadialForceVolumes().size();
    snapshot.physicsConstraintVolumes = environmentService.GetPhysicsConstraintVolumes().size();
    snapshot.kinematicTranslationPrimitives = environmentService.GetKinematicTranslationPrimitives().size();
    snapshot.kinematicRotationPrimitives = environmentService.GetKinematicRotationPrimitives().size();
    snapshot.cameraBlockingVolumes = environmentService.GetCameraBlockingVolumes().size();
    snapshot.aiPerceptionBlockerVolumes = environmentService.GetAiPerceptionBlockerVolumes().size();
    snapshot.safeZoneVolumes = environmentService.GetSafeZoneVolumes().size();
    snapshot.traversalLinkVolumes = environmentService.GetTraversalLinkVolumes().size();
    snapshot.pivotAnchorPrimitives = environmentService.GetPivotAnchorPrimitives().size();
    snapshot.symmetryMirrorPlanes = environmentService.GetSymmetryMirrorPlanes().size();
    snapshot.localGridSnapVolumes = environmentService.GetLocalGridSnapVolumes().size();
    snapshot.hintPartitionVolumes = environmentService.GetHintPartitionVolumes().size();
    snapshot.doorWindowCutoutPrimitives = environmentService.GetDoorWindowCutoutPrimitives().size();
    snapshot.cameraConfinementVolumes = environmentService.GetCameraConfinementVolumes().size();
    snapshot.lodOverrideVolumes = environmentService.GetLodOverrideVolumes().size();
    snapshot.lodSwitchPrimitives = environmentService.GetLodSwitchPrimitives().size();
    std::uint64_t lodSwitches = 0U;
    std::uint64_t lodThrashWarnings = 0U;
    for (const LodSwitchSelectionState& state : environmentService.GetLodSwitchSelectionStates()) {
        lodSwitches += state.switchCount;
        lodThrashWarnings += state.thrashWarnings;
    }
    snapshot.lodSwitchSwitches = static_cast<std::size_t>(lodSwitches);
    snapshot.lodSwitchThrashWarnings = static_cast<std::size_t>(lodThrashWarnings);
    snapshot.surfaceScatterVolumes = environmentService.GetSurfaceScatterVolumes().size();
    std::size_t activeSurfaceScatterCount = 0U;
    for (const SurfaceScatterRuntimeState& state : environmentService.GetSurfaceScatterRuntimeStates({0.0f, 0.0f, 0.0f})) {
        activeSurfaceScatterCount += static_cast<std::size_t>(state.generatedCount);
    }
    snapshot.surfaceScatterActiveInstances = activeSurfaceScatterCount;
    snapshot.splineMeshDeformers = environmentService.GetSplineMeshDeformerPrimitives().size();
    std::size_t splineMeshSegments = 0U;
    for (const SplineMeshDeformerRuntimeState& state :
         environmentService.GetSplineMeshDeformerRuntimeStates({0.0f, 0.0f, 0.0f})) {
        splineMeshSegments += static_cast<std::size_t>(state.generatedSegments);
    }
    snapshot.splineMeshDeformerSegments = splineMeshSegments;
    snapshot.splineDecalRibbons = environmentService.GetSplineDecalRibbonPrimitives().size();
    std::size_t splineRibbonTriangles = 0U;
    for (const SplineDecalRibbonRuntimeState& state :
         environmentService.GetSplineDecalRibbonRuntimeStates({0.0f, 0.0f, 0.0f})) {
        splineRibbonTriangles += static_cast<std::size_t>(state.generatedTriangles);
    }
    snapshot.splineDecalRibbonTriangles = splineRibbonTriangles;
    snapshot.topologicalUvRemappers = environmentService.GetTopologicalUvRemapperVolumes().size();
    snapshot.triPlanarNodes = environmentService.GetTriPlanarNodes().size();
    std::size_t proceduralUvPatches = 0U;
    for (const ProceduralUvProjectionRuntimeState& state :
         environmentService.GetProceduralUvProjectionRuntimeStates({0.0f, 0.0f, 0.0f})) {
        proceduralUvPatches += static_cast<std::size_t>(state.estimatedMaterialPatches);
    }
    snapshot.proceduralUvProjectionEstimatedPatches = proceduralUvPatches;
    snapshot.instanceCloudPrimitives = environmentService.GetInstanceCloudPrimitives().size();
    std::size_t activeInstanceCloudCount = 0U;
    for (const InstanceCloudRuntimeState& state : environmentService.GetInstanceCloudRuntimeStates({0.0f, 0.0f, 0.0f})) {
        activeInstanceCloudCount += static_cast<std::size_t>(state.activeInstanceCount);
    }
    snapshot.instanceCloudActiveInstances = activeInstanceCloudCount;
    snapshot.voronoiFracturePrimitives = environmentService.GetVoronoiFracturePrimitives().size();
    snapshot.metaballPrimitives = environmentService.GetMetaballPrimitives().size();
    snapshot.latticeVolumes = environmentService.GetLatticeVolumes().size();
    snapshot.manifoldSweepPrimitives = environmentService.GetManifoldSweepPrimitives().size();
    snapshot.trimSheetSweepPrimitives = environmentService.GetTrimSheetSweepPrimitives().size();
    snapshot.lSystemBranchPrimitives = environmentService.GetLSystemBranchPrimitives().size();
    snapshot.geodesicSpherePrimitives = environmentService.GetGeodesicSpherePrimitives().size();
    snapshot.extrudeAlongNormalPrimitives = environmentService.GetExtrudeAlongNormalPrimitives().size();
    snapshot.superellipsoidPrimitives = environmentService.GetSuperellipsoidPrimitives().size();
    snapshot.primitiveDemoLatticePrimitives = environmentService.GetPrimitiveDemoLatticePrimitives().size();
    snapshot.primitiveDemoVoronoiPrimitives = environmentService.GetPrimitiveDemoVoronoiPrimitives().size();
    snapshot.thickPolygonPrimitives = environmentService.GetThickPolygonPrimitives().size();
    snapshot.structuralProfilePrimitives = environmentService.GetStructuralProfilePrimitives().size();
    snapshot.halfPipePrimitives = environmentService.GetHalfPipePrimitives().size();
    snapshot.quarterPipePrimitives = environmentService.GetQuarterPipePrimitives().size();
    snapshot.pipeElbowPrimitives = environmentService.GetPipeElbowPrimitives().size();
    snapshot.torusSlicePrimitives = environmentService.GetTorusSlicePrimitives().size();
    snapshot.splineSweepPrimitives = environmentService.GetSplineSweepPrimitives().size();
    snapshot.revolvePrimitives = environmentService.GetRevolvePrimitives().size();
    snapshot.domeVaultPrimitives = environmentService.GetDomeVaultPrimitives().size();
    snapshot.loftPrimitives = environmentService.GetLoftPrimitives().size();
    snapshot.navmeshModifierVolumes = environmentService.GetNavmeshModifierVolumes().size();
    snapshot.reflectionProbeVolumes = environmentService.GetReflectionProbeVolumes().size();
    snapshot.lightImportanceVolumes = environmentService.GetLightImportanceVolumes().size();
    snapshot.lightPortalVolumes = environmentService.GetLightPortalVolumes().size();
    snapshot.voxelGiBoundsVolumes = environmentService.GetVoxelGiBoundsVolumes().size();
    snapshot.lightmapDensityVolumes = environmentService.GetLightmapDensityVolumes().size();
    snapshot.shadowExclusionVolumes = environmentService.GetShadowExclusionVolumes().size();
    snapshot.cullingDistanceVolumes = environmentService.GetCullingDistanceVolumes().size();
    snapshot.referenceImagePlanes = environmentService.GetReferenceImagePlanes().size();
    snapshot.text3dPrimitives = environmentService.GetText3dPrimitives().size();
    snapshot.annotationCommentPrimitives = environmentService.GetAnnotationCommentPrimitives().size();
    snapshot.measureToolPrimitives = environmentService.GetMeasureToolPrimitives().size();
    snapshot.renderTargetSurfaces = environmentService.GetRenderTargetSurfaces().size();
    snapshot.planarReflectionSurfaces = environmentService.GetPlanarReflectionSurfaces().size();
    snapshot.passThroughPrimitives = environmentService.GetPassThroughPrimitives().size();
    snapshot.skyProjectionSurfaces = environmentService.GetSkyProjectionSurfaces().size();
    snapshot.infoPanelSpawners = environmentService.GetDynamicInfoPanelSpawners().size();
    snapshot.volumetricEmitterBounds = environmentService.GetVolumetricEmitterBounds().size();
    snapshot.splinePathFollowerPrimitives = environmentService.GetSplinePathFollowerPrimitives().size();
    snapshot.cablePrimitives = environmentService.GetCablePrimitives().size();
    snapshot.clippingVolumes = environmentService.GetClippingVolumes().size();
    snapshot.filteredCollisionVolumes = environmentService.GetFilteredCollisionVolumes().size();
    snapshot.portalPrimitives = environmentService.CountVisibilityPrimitives(VisibilityPrimitiveKind::Portal);
    snapshot.antiPortalPrimitives = environmentService.CountVisibilityPrimitives(VisibilityPrimitiveKind::AntiPortal);
    snapshot.occlusionPortalVolumes = environmentService.GetOcclusionPortalVolumes().size();
    snapshot.closedOcclusionPortals = environmentService.CountClosedOcclusionPortals();
    snapshot.genericTriggerVolumes = environmentService.GetGenericTriggerVolumes().size();
    snapshot.spatialQueryVolumes = environmentService.GetSpatialQueryVolumes().size();
    snapshot.streamingLevelVolumes = environmentService.GetStreamingLevelVolumes().size();
    snapshot.checkpointSpawnVolumes = environmentService.GetCheckpointSpawnVolumes().size();
    snapshot.teleportVolumes = environmentService.GetTeleportVolumes().size();
    snapshot.launchVolumes = environmentService.GetLaunchVolumes().size();
    snapshot.analyticsHeatmapVolumes = environmentService.GetAnalyticsHeatmapVolumes().size();

    if (structuralSummary.has_value()) {
        snapshot.structuralGraphNodes = structuralSummary->nodeCount;
        snapshot.structuralGraphEdges = structuralSummary->edgeCount;
        snapshot.structuralGraphCycles = structuralSummary->cycleCount;
        snapshot.structuralGraphUnresolvedDependencies = structuralSummary->unresolvedDependencyCount;
        snapshot.structuralGraphCompilePhase = structuralSummary->phaseBuckets.compile;
        snapshot.structuralGraphRuntimePhase = structuralSummary->phaseBuckets.runtime;
        snapshot.structuralGraphPostBuildPhase = structuralSummary->phaseBuckets.postBuild;
        snapshot.structuralGraphFramePhase = structuralSummary->phaseBuckets.frame;
    }
    if (deferredPipelineResult.has_value()) {
        snapshot.structuralDeferredOperations =
            deferredPipelineResult->deferredExecution.operationStats.size();
        snapshot.structuralDeferredUnsupportedOperations =
            deferredPipelineResult->unsupportedOperationIds.size();
        if (snapshot.structuralDeferredUnsupportedOperations > 0U) {
            snapshot.structuralDeferredHealth = "unsupported";
        } else {
            const bool hasWarnings = std::any_of(
                deferredPipelineResult->deferredExecution.operationStats.begin(),
                deferredPipelineResult->deferredExecution.operationStats.end(),
                [](const ri::structural::StructuralDeferredOperationStats& stats) {
                    return !stats.succeeded;
                });
            snapshot.structuralDeferredHealth = hasWarnings ? "warnings" : "ok";
        }
        snapshot.structuralDeferredStatusLine = snapshot.structuralDeferredHealth
            + " (" + std::to_string(snapshot.structuralDeferredUnsupportedOperations)
            + "/" + std::to_string(snapshot.structuralDeferredOperations) + ")";
        snapshot.structuralDeferredSummary = SummarizeHelperActivity(
            ri::structural::BuildStructuralDeferredPipelineReport(*deferredPipelineResult),
            64);
    }

    snapshot.activePostProcess = activePostProcessState.label;
    snapshot.activePostProcessTint = activePostProcessState.tintStrength;
    snapshot.statsOverlayEnabled = statsMetrics.visible;
    snapshot.statsOverlayAttached = statsMetrics.attached;
    snapshot.statsOverlayFrameTimeMs = statsMetrics.frameTimeMs;
    snapshot.statsOverlayFramesPerSecond = statsMetrics.framesPerSecond;
    return snapshot;
}

void RuntimeDiagnosticsLayer::SetVisible(const bool visible) {
    if (visible_ == visible) {
        return;
    }
    visible_ = visible;
    revision_ += 1U;
}

void RuntimeDiagnosticsLayer::ToggleVisible() {
    SetVisible(!visible_);
}

bool RuntimeDiagnosticsLayer::IsVisible() const {
    return visible_;
}

void RuntimeDiagnosticsLayer::SetDebugHelpersVisible(const bool visible) {
    SetVisible(visible);
}

void RuntimeDiagnosticsLayer::ToggleDebugHelpersVisible() {
    ToggleVisible();
}

bool RuntimeDiagnosticsLayer::DebugHelpersVisible() const {
    return IsVisible();
}

void RuntimeDiagnosticsLayer::SetDebugHelpersRoot(std::string root) {
    if (root.empty()) {
        root = "runtime.world";
    }
    if (debugHelpersRoot_ == root) {
        return;
    }
    debugHelpersRoot_ = std::move(root);
    revision_ += 1U;
}

const std::string& RuntimeDiagnosticsLayer::GetDebugHelpersRoot() const {
    return debugHelpersRoot_;
}

namespace {

template <typename TVolume>
void AppendRuntimeVolumeHelpers(std::vector<RuntimeDiagnosticsHelper>& helpers, const std::vector<TVolume>& volumes) {
    for (const TVolume& source : volumes) {
        if (!source.debugVisible) {
            continue;
        }
        RuntimeDiagnosticsHelper helper{};
        helper.sourceId = source.id;
        helper.sourceType = source.type;
        helper.volume = static_cast<const RuntimeVolume&>(source);
        helpers.push_back(std::move(helper));
    }
}

void AppendVisibilityHelpers(std::vector<RuntimeDiagnosticsHelper>& helpers, const std::vector<VisibilityPrimitive>& volumes) {
    for (const VisibilityPrimitive& source : volumes) {
        if (!source.debugVisible) {
            continue;
        }
        RuntimeDiagnosticsHelper helper{};
        helper.sourceId = source.id;
        helper.sourceType = source.type;
        helper.volume.id = source.id;
        helper.volume.type = source.type;
        helper.volume.debugVisible = source.debugVisible;
        helper.volume.shape = VolumeShape::Box;
        helper.volume.position = source.position;
        helper.volume.size = source.size;
        helper.volume.radius = std::max(source.size.x, source.size.z) * 0.5f;
        helper.volume.height = source.size.y;
        helpers.push_back(std::move(helper));
    }
}

void AppendTeleportGizmos(std::vector<RuntimeDiagnosticsHelper>& helpers, const std::vector<TeleportVolume>& volumes) {
    for (const TeleportVolume& source : volumes) {
        if (!source.debugVisible) {
            continue;
        }
        RuntimeDiagnosticsHelper helper{};
        helper.sourceId = source.id;
        helper.sourceType = source.type;
        helper.volume = static_cast<const RuntimeVolume&>(source);
        helper.gizmoLineStart = source.position;
        helper.gizmoLineEnd = source.targetPosition;
        helpers.push_back(std::move(helper));
    }
}

void AppendLaunchGizmos(std::vector<RuntimeDiagnosticsHelper>& helpers, const std::vector<LaunchVolume>& volumes) {
    for (const LaunchVolume& source : volumes) {
        if (!source.debugVisible) {
            continue;
        }
        RuntimeDiagnosticsHelper helper{};
        helper.sourceId = source.id;
        helper.sourceType = source.type;
        helper.volume = static_cast<const RuntimeVolume&>(source);
        helper.gizmoLineStart = source.position;
        helper.gizmoLineEnd = source.position + source.impulse;
        helpers.push_back(std::move(helper));
    }
}

} // namespace

void RuntimeDiagnosticsLayer::Rebuild(const RuntimeEnvironmentService& environment) {
    helpers_.clear();
    AppendRuntimeVolumeHelpers(helpers_, environment.GetPostProcessVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetAudioReverbVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetAudioOcclusionVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetAmbientAudioVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetLocalizedFogVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetFogBlockerVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetFluidSimulationVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetPhysicsModifierVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetSurfaceVelocityPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetWaterSurfacePrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetRadialForceVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetPhysicsConstraintVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetKinematicTranslationPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetKinematicRotationPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetCameraBlockingVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetAiPerceptionBlockerVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetSafeZoneVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetTraversalLinkVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetPivotAnchorPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetSymmetryMirrorPlanes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetLocalGridSnapVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetHintPartitionVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetDoorWindowCutoutPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetCameraConfinementVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetLodOverrideVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetLodSwitchPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetSurfaceScatterVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetSplineMeshDeformerPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetSplineDecalRibbonPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetTopologicalUvRemapperVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetTriPlanarNodes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetInstanceCloudPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetVoronoiFracturePrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetMetaballPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetLatticeVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetManifoldSweepPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetTrimSheetSweepPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetLSystemBranchPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetGeodesicSpherePrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetExtrudeAlongNormalPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetSuperellipsoidPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetPrimitiveDemoLatticePrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetPrimitiveDemoVoronoiPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetThickPolygonPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetStructuralProfilePrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetHalfPipePrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetQuarterPipePrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetPipeElbowPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetTorusSlicePrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetSplineSweepPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetRevolvePrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetDomeVaultPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetLoftPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetNavmeshModifierVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetReflectionProbeVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetLightImportanceVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetLightPortalVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetVoxelGiBoundsVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetLightmapDensityVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetShadowExclusionVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetCullingDistanceVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetReferenceImagePlanes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetText3dPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetAnnotationCommentPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetMeasureToolPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetRenderTargetSurfaces());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetPlanarReflectionSurfaces());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetPassThroughPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetSkyProjectionSurfaces());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetVolumetricEmitterBounds());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetSplinePathFollowerPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetCablePrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetClippingVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetFilteredCollisionVolumes());
    AppendVisibilityHelpers(helpers_, environment.GetVisibilityPrimitives());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetOcclusionPortalVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetGenericTriggerVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetSpatialQueryVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetStreamingLevelVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetCheckpointSpawnVolumes());
    AppendRuntimeVolumeHelpers(helpers_, environment.GetAnalyticsHeatmapVolumes());
    AppendTeleportGizmos(helpers_, environment.GetTeleportVolumes());
    AppendLaunchGizmos(helpers_, environment.GetLaunchVolumes());
    revision_ += 1U;
}

RuntimeDiagnosticsSnapshot RuntimeDiagnosticsLayer::Snapshot() const {
    RuntimeDiagnosticsSnapshot snapshot{};
    snapshot.visible = visible_;
    snapshot.debugHelpersVisible = visible_;
    snapshot.debugHelpersRoot = debugHelpersRoot_;
    snapshot.revision = revision_;
    snapshot.helpers = helpers_;
    return snapshot;
}

ri::render::PostProcessParameters BuildPostProcessParameters(
    const PostProcessState& state,
    double timeSeconds,
    float staticFadeAmount) {
    if (!state.enabled) {
        return ri::render::SanitizePostProcessParameters(ri::render::PostProcessParameters{
            .timeSeconds = std::isfinite(timeSeconds) ? static_cast<float>(timeSeconds) : 0.0f,
            .staticFadeAmount = staticFadeAmount,
        });
    }
    return ri::render::SanitizePostProcessParameters(ri::render::PostProcessParameters{
        .timeSeconds = std::isfinite(timeSeconds) ? static_cast<float>(timeSeconds) : 0.0f,
        .noiseAmount = state.noiseAmount,
        .scanlineAmount = state.scanlineAmount,
        .barrelDistortion = state.barrelDistortion,
        .chromaticAberration = state.chromaticAberration,
        .tintColor = state.tintColor,
        .tintStrength = state.tintStrength,
        .blurAmount = state.blurAmount,
        .staticFadeAmount = staticFadeAmount,
    });
}

PostProcessState SetPostProcessingEnabled(const PostProcessState& state, bool enabled) {
    PostProcessState out = state;
    out.enabled = enabled;
    if (!enabled) {
        out.label = "disabled";
    } else if (out.label == "disabled") {
        out.label = out.activeVolumes.empty() ? "none" : "custom";
    }
    return out;
}

PostProcessState ApplyPostProcessPhase(const PostProcessState& state, std::string_view phase) {
    PostProcessState out = state;
    if (!out.enabled) {
        return out;
    }
    if (phase == "menu") {
        out.noiseAmount = std::max(out.noiseAmount, 0.006f);
        out.scanlineAmount = std::max(out.scanlineAmount, 0.004f);
        out.blurAmount = std::max(out.blurAmount, 0.001f);
    } else if (phase == "gameplay") {
        out.noiseAmount = std::max(out.noiseAmount, 0.0015f);
        out.scanlineAmount = std::max(out.scanlineAmount, 0.001f);
        out.blurAmount = std::min(out.blurAmount, 0.003f);
    } else if (phase == "transition") {
        out.noiseAmount = std::max(out.noiseAmount, 0.03f);
        out.scanlineAmount = std::max(out.scanlineAmount, 0.02f);
    }
    if (out.label == "none" || out.label == "disabled") {
        out.label = std::string(phase);
    }
    return out;
}

} // namespace ri::world
