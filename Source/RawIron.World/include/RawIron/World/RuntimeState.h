#pragma once

#include "RawIron/Audio/AudioManager.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Render/PostProcessProfiles.h"
#include "RawIron/Runtime/RuntimeEventBus.h"
#include "RawIron/Spatial/SpatialIndex.h"
#include "RawIron/Structural/StructuralDeferredOperations.h"
#include "RawIron/Structural/StructuralGraph.h"
#include "RawIron/Validation/Schemas.h"
#include "RawIron/World/InfoPanel.h"
#include "RawIron/World/HelperActivitySummary.h"
#include "RawIron/Logic/LogicTypes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ri::logic {
class LogicGraph;
}

namespace ri::world {

class SpatialQueryTracker;

enum class VolumeShape {
    Box,
    Cylinder,
    Sphere,
};

struct RuntimeVolume {
    std::string id;
    std::string type = "volume";
    bool debugVisible = true;
    VolumeShape shape = VolumeShape::Box;
    ri::math::Vec3 position{};
    ri::math::Vec3 size{1.0f, 1.0f, 1.0f};
    float radius = 0.5f;
    float height = 1.0f;
};

struct PostProcessVolume : RuntimeVolume {
    ri::math::Vec3 tintColor{1.0f, 1.0f, 1.0f};
    float tintStrength = 0.0f;
    float blurAmount = 0.0f;
    float noiseAmount = 0.0f;
    float scanlineAmount = 0.0f;
    float barrelDistortion = 0.0f;
    float chromaticAberration = 0.0f;
};

struct AudioReverbVolume : RuntimeVolume {
    float reverbMix = 0.0f;
    float echoDelayMs = 0.0f;
    float echoFeedback = 0.0f;
    float dampening = 0.0f;
    float volumeScale = 1.0f;
    float playbackRate = 1.0f;
};

struct AudioOcclusionVolume : RuntimeVolume {
    float occlusionStrength = 0.0f;
    float volumeScale = 1.0f;
};

struct LocalizedFogVolume : RuntimeVolume {
    ri::math::Vec3 tintColor{1.0f, 1.0f, 1.0f};
    float tintStrength = 0.0f;
    float blurAmount = 0.0f;
};

struct FogBlockerVolume : RuntimeVolume {};

struct FluidSimulationVolume : RuntimeVolume {
    float gravityScale = 0.35f;
    float jumpScale = 0.72f;
    float drag = 1.8f;
    float buoyancy = 0.9f;
    ri::math::Vec3 flow{};
    ri::math::Vec3 tintColor{1.0f, 1.0f, 1.0f};
    float tintStrength = 0.0f;
    float reverbMix = 0.0f;
    float echoDelayMs = 0.0f;
};

struct PhysicsModifierVolume : RuntimeVolume {
    float gravityScale = 1.0f;
    float jumpScale = 1.0f;
    float drag = 0.0f;
    float buoyancy = 0.0f;
    ri::math::Vec3 flow{};
};

struct SurfaceVelocityPrimitive : RuntimeVolume {
    ri::math::Vec3 flow{};
};

struct WaterSurfacePrimitive : RuntimeVolume {
    float waveAmplitude = 0.08f;
    float waveFrequency = 0.6f;
    float flowSpeed = 0.0f;
    bool blocksUnderwaterFog = false;
};

struct RadialForceVolume : RuntimeVolume {
    float strength = 4.2f;
    float falloff = 1.0f;
    float innerRadius = 0.0f;
};

enum class ConstraintAxis {
    X,
    Y,
    Z,
};

struct PhysicsConstraintVolume : RuntimeVolume {
    std::vector<ConstraintAxis> lockAxes;
};

struct KinematicTranslationPrimitive : RuntimeVolume {
    ri::math::Vec3 axis{1.0f, 0.0f, 0.0f};
    float distance = 2.0f;
    float cycleSeconds = 3.0f;
    bool pingPong = true;
};

struct KinematicRotationPrimitive : RuntimeVolume {
    ri::math::Vec3 axis{0.0f, 1.0f, 0.0f};
    float angularSpeedDegreesPerSecond = 45.0f;
    float maxAngleDegrees = 360.0f;
    bool pingPong = false;
};

enum class TraversalLinkKind {
    General,
    Ladder,
    Climb,
};

enum class HintPartitionMode {
    Hint,
    Skip,
};

enum class ForcedLod {
    Near,
    Far,
};

struct TraversalLinkVolume : RuntimeVolume {
    TraversalLinkKind kind = TraversalLinkKind::General;
    float climbSpeed = 3.4f;
};

struct PivotAnchorPrimitive : RuntimeVolume {
    std::string anchorId;
    ri::math::Vec3 forwardAxis{0.0f, 0.0f, 1.0f};
    bool alignToSurfaceNormal = false;
};

struct SymmetryMirrorPlane : RuntimeVolume {
    ri::math::Vec3 planeNormal{1.0f, 0.0f, 0.0f};
    float planeOffset = 0.0f;
    bool keepOriginal = true;
    bool snapToGrid = false;
};

struct LocalGridSnapVolume : RuntimeVolume {
    float snapSize = 0.5f;
    bool snapX = true;
    bool snapY = true;
    bool snapZ = true;
    int priority = 0;
};

struct HintPartitionVolume : RuntimeVolume {
    HintPartitionMode mode = HintPartitionMode::Hint;
};

struct HintPartitionState {
    const HintPartitionVolume* volume = nullptr;
    HintPartitionMode mode = HintPartitionMode::Hint;
    bool inside = false;
};

struct SafeZoneRuntimeVolume : RuntimeVolume {
    bool dropAggro = true;
};

struct SafeZoneState {
    bool inside = false;
    bool dropAggro = false;
    std::vector<const SafeZoneRuntimeVolume*> matches{};
};

struct CameraConfinementVolume : RuntimeVolume {};

struct CameraBlockingVolume : RuntimeVolume {
    std::vector<std::string> channels{"camera"};
};

struct AiPerceptionBlockerVolume : RuntimeVolume {
    std::vector<std::string> modes{"ai"};
    bool enabled = true;
};

struct LodOverrideVolume : RuntimeVolume {
    std::vector<std::string> targetIds;
    ForcedLod forcedLod = ForcedLod::Near;
};

struct LodOverrideSelectionState {
    std::vector<const LodOverrideVolume*> matches{};
    const LodOverrideVolume* selected = nullptr;
    bool hasTargetMatch = false;
    ForcedLod forcedLod = ForcedLod::Near;
};

struct DoorWindowCutoutPrimitive : RuntimeVolume {
    float openingWidth = 2.0f;
    float openingHeight = 2.4f;
    float sillHeight = 0.0f;
    float lintelHeight = 2.4f;
    bool carveCollision = true;
    bool carveVisual = true;
};

struct ProceduralDoorEntity : RuntimeVolume {
    float openingWidth = 1.2f;
    float openingHeight = 2.2f;
    float thickness = 0.2f;
    bool startsOpen = false;
    bool startsLocked = false;
    bool blocksWhileClosed = true;
    std::string interactionPrompt = "Use Door";
    std::string deniedPrompt = "Locked";
    std::string interactionHook;
    std::string transitionLevel;
    std::string endingTrigger;
    std::string accessFeedbackTag;
};

struct DoorTransitionRequest {
    std::string doorId;
    std::string transitionLevel;
    std::string endingTrigger;
    std::string accessFeedbackTag;
};

struct AiPerceptionBlockerState {
    bool blocked = false;
    bool anyEnabled = false;
    std::vector<const AiPerceptionBlockerVolume*> matches{};
};

enum class LodSwitchRepresentationKind {
    Primitive,
    Mesh,
    Cluster,
};

enum class LodSwitchCollisionProfile {
    Full,
    Simplified,
    None,
};

enum class LodSwitchMetric {
    CameraDistance,
    ScreenSize,
};

enum class LodSwitchTransitionMode {
    Hard,
    Crossfade,
};

struct LodSwitchRepresentation {
    LodSwitchRepresentationKind kind = LodSwitchRepresentationKind::Primitive;
    std::string payloadId;
};

struct LodSwitchLevel {
    std::string name;
    LodSwitchRepresentation representation{};
    LodSwitchCollisionProfile collisionProfile = LodSwitchCollisionProfile::Full;
    float distanceEnter = 0.0f;
    float distanceExit = 0.0f;
};

struct LodSwitchPolicy {
    LodSwitchMetric metric = LodSwitchMetric::CameraDistance;
    bool hysteresisEnabled = true;
    LodSwitchTransitionMode transitionMode = LodSwitchTransitionMode::Hard;
    float crossfadeSeconds = 0.0f;
};

struct LodSwitchDebugSettings {
    bool showActiveLevel = false;
    bool showRanges = false;
};

struct LodSwitchPrimitive : RuntimeVolume {
    std::vector<LodSwitchLevel> levels;
    LodSwitchPolicy policy{};
    LodSwitchDebugSettings debug{};
    std::size_t activeLevelIndex = 0U;
    std::size_t previousLevelIndex = 0U;
    float crossfadeAlpha = 1.0f;
    std::uint64_t switchCount = 0U;
    std::uint64_t thrashWarnings = 0U;
    double lastSwitchTimeSeconds = -1.0;
};

struct LodSwitchSelectionState {
    std::string id;
    std::string activeLevel;
    std::string previousLevel;
    LodSwitchCollisionProfile collisionProfile = LodSwitchCollisionProfile::None;
    bool crossfadeActive = false;
    float crossfadeAlpha = 1.0f;
    std::uint64_t switchCount = 0U;
    std::uint64_t thrashWarnings = 0U;
};

enum class SurfaceScatterRepresentationKind {
    Primitive,
    Mesh,
    Cluster,
};

enum class SurfaceScatterCollisionPolicy {
    None,
    Proxy,
    Full,
};

struct SurfaceScatterSourceRepresentation {
    SurfaceScatterRepresentationKind kind = SurfaceScatterRepresentationKind::Primitive;
    std::string payloadId;
};

struct SurfaceScatterDensityControls {
    std::uint32_t count = 64U;
    float densityPerSquareMeter = 0.0f;
    std::uint32_t maxPoints = 2048U;
};

struct SurfaceScatterDistributionControls {
    std::uint32_t seed = 1337U;
    float minSlopeDegrees = 0.0f;
    float maxSlopeDegrees = 85.0f;
    float minHeight = -100000.0f;
    float maxHeight = 100000.0f;
    float minNormalY = 0.0f;
    float minSeparation = 0.0f;
    ri::math::Vec3 rotationJitterRadians{};
    ri::math::Vec3 scaleJitter{};
    ri::math::Vec3 positionJitter{};
};

struct SurfaceScatterCullingPolicy {
    float maxActiveDistance = 80.0f;
    bool frustumCulling = true;
};

struct SurfaceScatterAnimationSettings {
    bool windSwayEnabled = false;
    float swayAmplitude = 0.0f;
    float swayFrequency = 0.0f;
};

struct SurfaceScatterVolume : RuntimeVolume {
    std::vector<std::string> targetIds;
    SurfaceScatterSourceRepresentation sourceRepresentation{};
    SurfaceScatterDensityControls density{};
    SurfaceScatterDistributionControls distribution{};
    SurfaceScatterCollisionPolicy collisionPolicy = SurfaceScatterCollisionPolicy::None;
    SurfaceScatterCullingPolicy culling{};
    SurfaceScatterAnimationSettings animation{};
};

struct SurfaceScatterRuntimeState {
    std::string id;
    bool active = false;
    bool withinDistance = false;
    bool targetsResolved = false;
    std::uint32_t requestedCount = 0U;
    std::uint32_t generatedCount = 0U;
    std::uint64_t layoutSignature = 0U;
    SurfaceScatterCollisionPolicy collisionPolicy = SurfaceScatterCollisionPolicy::None;
};

struct SplineMeshDeformerPrimitive : RuntimeVolume {
    std::vector<std::string> targetIds;
    std::vector<ri::math::Vec3> splinePoints;
    std::uint32_t sampleCount = 16U;
    std::uint32_t sectionCount = 1U;
    float segmentLength = 2.0f;
    float tangentSmoothing = 0.5f;
    bool keepSource = false;
    bool collisionEnabled = false;
    bool navInfluence = false;
    bool dynamicEnabled = false;
    std::uint32_t seed = 1337U;
    std::uint32_t maxSamples = 256U;
    float maxActiveDistance = 120.0f;
    bool frustumCulling = true;
};

struct SplineMeshDeformerRuntimeState {
    std::string id;
    bool active = false;
    bool withinDistance = false;
    bool splineValid = false;
    bool targetsResolved = false;
    std::uint32_t requestedSamples = 0U;
    std::uint32_t generatedSegments = 0U;
    std::uint64_t topologySignature = 0U;
};

struct SplineDecalRibbonPrimitive : RuntimeVolume {
    std::vector<ri::math::Vec3> splinePoints;
    float width = 1.0f;
    std::uint32_t tessellation = 32U;
    float offsetY = 0.03f;
    float uvScaleU = 1.0f;
    float uvScaleV = 1.0f;
    float tangentSmoothing = 0.5f;
    bool transparentBlend = true;
    bool depthWrite = false;
    bool collisionEnabled = false;
    bool navInfluence = false;
    bool dynamicEnabled = false;
    std::uint32_t seed = 1337U;
    std::uint32_t maxSamples = 256U;
    float maxActiveDistance = 120.0f;
    bool frustumCulling = true;
};

struct SplineDecalRibbonRuntimeState {
    std::string id;
    bool active = false;
    bool withinDistance = false;
    bool splineValid = false;
    std::uint32_t requestedSamples = 0U;
    std::uint32_t generatedSegments = 0U;
    std::uint32_t generatedTriangles = 0U;
    std::uint64_t topologySignature = 0U;
};

struct ProceduralUvProjectionDebugControls {
    bool previewTint = false;
    bool targetOutlines = false;
    bool axisContributionPreview = false;
    bool texelDensityPreview = false;
};

struct TopologicalUvRemapperVolume : RuntimeVolume {
    std::vector<std::string> targetIds;
    std::string remapMode = "triplanar";
    std::string textureX;
    std::string textureY;
    std::string textureZ;
    std::string sharedTextureId;
    float projectionScale = 1.0f;
    float blendSharpness = 4.0f;
    ri::math::Vec3 axisWeights{1.0f, 1.0f, 1.0f};
    std::uint32_t maxMaterialPatches = 256U;
    float maxActiveDistance = 512.0f;
    bool frustumCulling = true;
    ProceduralUvProjectionDebugControls debug{};
};

struct TriPlanarNode : RuntimeVolume {
    std::vector<std::string> targetIds;
    std::string textureX;
    std::string textureY;
    std::string textureZ;
    std::string sharedTextureId;
    float projectionScale = 1.0f;
    float blendSharpness = 4.0f;
    ri::math::Vec3 axisWeights{1.0f, 1.0f, 1.0f};
    std::uint32_t maxMaterialPatches = 256U;
    bool objectSpaceAxes = false;
    float maxActiveDistance = 512.0f;
    bool frustumCulling = true;
    ProceduralUvProjectionDebugControls debug{};
};

enum class ProceduralUvProjectionKind {
    TopologicalRemapper,
    TriPlanarNode,
};

struct ProceduralUvProjectionRuntimeState {
    std::string id;
    ProceduralUvProjectionKind kind = ProceduralUvProjectionKind::TopologicalRemapper;
    bool active = false;
    bool withinDistance = false;
    bool targetSetValid = false;
    bool textureSetValid = false;
    std::uint32_t estimatedMaterialPatches = 0U;
    std::uint64_t configSignature = 0U;
};

enum class InstanceCloudRepresentationKind {
    Primitive,
    Mesh,
    Cluster,
};

enum class InstanceCloudCollisionPolicy {
    None,
    Simplified,
    PerInstance,
};

struct InstanceCloudSourceRepresentation {
    InstanceCloudRepresentationKind kind = InstanceCloudRepresentationKind::Primitive;
    std::string payloadId;
};

struct InstanceCloudVariationRanges {
    ri::math::Vec3 rotationJitterRadians{};
    ri::math::Vec3 scaleJitter{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 positionJitter{};
};

struct InstanceCloudCullingPolicy {
    float maxActiveDistance = 80.0f;
    bool frustumCulling = true;
};

struct InstanceCloudPrimitive : RuntimeVolume {
    InstanceCloudSourceRepresentation sourceRepresentation{};
    std::uint32_t count = 1U;
    ri::math::Vec3 offsetStep{};
    ri::math::Vec3 distributionExtents{};
    std::uint32_t seed = 1337U;
    InstanceCloudVariationRanges variation{};
    InstanceCloudCollisionPolicy collisionPolicy = InstanceCloudCollisionPolicy::None;
    InstanceCloudCullingPolicy culling{};
};

struct InstanceCloudRuntimeState {
    std::string id;
    bool active = true;
    bool withinDistance = true;
    std::uint32_t instanceCount = 1U;
    std::uint32_t activeInstanceCount = 1U;
    float viewerDistance = 0.0f;
    InstanceCloudCollisionPolicy collisionPolicy = InstanceCloudCollisionPolicy::None;
};

struct VoronoiFracturePrimitive : RuntimeVolume {
    std::vector<std::string> targetIds;
    std::uint32_t cellCount = 16U;
    float noiseJitter = 0.1f;
    std::uint32_t seed = 1337U;
    bool capOpenFaces = true;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct MetaballPrimitive : RuntimeVolume {
    std::vector<ri::math::Vec3> controlPoints;
    float isoLevel = 0.5f;
    float smoothing = 0.35f;
    std::uint32_t resolution = 24U;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct LatticeVolume : RuntimeVolume {
    std::vector<std::string> targetIds;
    ri::math::Vec3 cellSize{0.5f, 0.5f, 0.5f};
    float beamRadius = 0.08f;
    std::uint32_t maxCells = 2048U;
    float maxActiveDistance = 120.0f;
    bool frustumCulling = true;
};

struct ManifoldSweepPrimitive : RuntimeVolume {
    std::vector<std::string> targetIds;
    std::vector<ri::math::Vec3> splinePoints;
    float profileRadius = 0.25f;
    std::uint32_t sampleCount = 32U;
    bool capEnds = true;
    float maxActiveDistance = 128.0f;
    bool frustumCulling = true;
};

struct TrimSheetSweepPrimitive : RuntimeVolume {
    std::vector<std::string> targetIds;
    std::vector<ri::math::Vec3> splinePoints;
    std::string trimSheetId;
    float uvTileU = 1.0f;
    float uvTileV = 1.0f;
    std::uint32_t tessellation = 24U;
    float maxActiveDistance = 128.0f;
    bool frustumCulling = true;
};

struct LSystemBranchPrimitive : RuntimeVolume {
    std::vector<std::string> targetIds;
    std::uint32_t iterations = 4U;
    float segmentLength = 0.5f;
    float branchAngleDegrees = 22.5f;
    std::uint32_t seed = 1337U;
    float maxActiveDistance = 120.0f;
    bool frustumCulling = true;
};

struct GeodesicSpherePrimitive : RuntimeVolume {
    std::uint32_t subdivisionLevel = 2U;
    float radiusScale = 1.0f;
    float maxActiveDistance = 120.0f;
    bool frustumCulling = true;
};

struct ExtrudeAlongNormalPrimitive : RuntimeVolume {
    std::vector<std::string> targetIds;
    float distance = 0.2f;
    std::uint32_t shellCount = 1U;
    bool capOpenEdges = true;
    float maxActiveDistance = 120.0f;
    bool frustumCulling = true;
};

struct SuperellipsoidPrimitive : RuntimeVolume {
    float exponentX = 2.0f;
    float exponentY = 2.0f;
    float exponentZ = 2.0f;
    std::uint32_t radialSegments = 24U;
    std::uint32_t rings = 16U;
    float maxActiveDistance = 120.0f;
    bool frustumCulling = true;
};

struct PrimitiveDemoLattice : RuntimeVolume {
    std::vector<std::string> targetIds;
    ri::math::Vec3 cellSize{0.6f, 0.6f, 0.6f};
    std::uint32_t maxCells = 1024U;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct PrimitiveDemoVoronoi : RuntimeVolume {
    std::vector<std::string> targetIds;
    std::uint32_t cellCount = 12U;
    float jitter = 0.1f;
    std::uint32_t seed = 1337U;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct ThickPolygonPrimitive : RuntimeVolume {
    std::vector<ri::math::Vec3> points;
    float thickness = 0.2f;
    bool capTop = true;
    bool capBottom = true;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct StructuralProfilePrimitive : RuntimeVolume {
    std::string profileId;
    float profileScale = 1.0f;
    std::uint32_t segmentCount = 16U;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct HalfPipePrimitive : RuntimeVolume {
    float radius = 2.0f;
    float length = 6.0f;
    std::uint32_t radialSegments = 16U;
    float wallThickness = 0.2f;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct QuarterPipePrimitive : RuntimeVolume {
    float radius = 2.0f;
    float length = 6.0f;
    std::uint32_t radialSegments = 12U;
    float wallThickness = 0.2f;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct PipeElbowPrimitive : RuntimeVolume {
    float radius = 1.0f;
    float bendDegrees = 90.0f;
    std::uint32_t radialSegments = 16U;
    std::uint32_t bendSegments = 12U;
    float wallThickness = 0.15f;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct TorusSlicePrimitive : RuntimeVolume {
    float majorRadius = 2.0f;
    float minorRadius = 0.5f;
    float sweepDegrees = 180.0f;
    std::uint32_t radialSegments = 24U;
    std::uint32_t tubularSegments = 16U;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct SplineSweepPrimitive : RuntimeVolume {
    std::vector<std::string> targetIds;
    std::vector<ri::math::Vec3> splinePoints;
    float profileRadius = 0.25f;
    std::uint32_t sampleCount = 32U;
    bool capEnds = true;
    float maxActiveDistance = 120.0f;
    bool frustumCulling = true;
};

struct RevolvePrimitive : RuntimeVolume {
    std::vector<ri::math::Vec3> profilePoints;
    float sweepDegrees = 360.0f;
    std::uint32_t segmentCount = 24U;
    bool capEnds = false;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct DomeVaultPrimitive : RuntimeVolume {
    float radius = 4.0f;
    float thickness = 0.25f;
    float heightRatio = 0.5f;
    std::uint32_t radialSegments = 24U;
    float maxActiveDistance = 96.0f;
    bool frustumCulling = true;
};

struct LoftPrimitive : RuntimeVolume {
    std::vector<ri::math::Vec3> pathPoints;
    std::vector<ri::math::Vec3> profilePoints;
    std::uint32_t segmentCount = 24U;
    bool capEnds = true;
    float maxActiveDistance = 120.0f;
    bool frustumCulling = true;
};

struct NavmeshModifierVolume : RuntimeVolume {
    float traversalCost = 1.5f;
    std::string tag = "modified";
};

enum class VisibilityPrimitiveKind {
    Portal,
    AntiPortal,
    OcclusionPortal,
};

struct VisibilityPrimitive {
    std::string id;
    std::string type = "portal";
    bool debugVisible = true;
    VisibilityPrimitiveKind kind = VisibilityPrimitiveKind::Portal;
    ri::math::Vec3 position{};
    ri::math::Vec3 rotationRadians{};
    ri::math::Vec3 size{1.0f, 1.0f, 1.0f};
    bool closed = false;
};

struct ReflectionProbeVolume : RuntimeVolume {
    float intensity = 1.0f;
    float blendDistance = 1.5f;
    std::uint32_t captureResolution = 256;
    bool boxProjection = true;
    bool dynamicCapture = false;
};

struct LightImportanceVolume : RuntimeVolume {
    bool probeGridBounds = false;
};

struct LightPortalVolume : RuntimeVolume {
    float transmission = 1.0f;
    float softness = 0.1f;
    float priority = 0.0f;
    bool twoSided = false;
};

struct VoxelGiBoundsVolume : RuntimeVolume {
    float voxelSize = 1.0f;
    std::uint32_t cascadeCount = 1U;
    bool updateDynamics = true;
};

struct LightmapDensityVolume : RuntimeVolume {
    float texelsPerMeter = 256.0f;
    float minimumTexelsPerMeter = 64.0f;
    float maximumTexelsPerMeter = 1024.0f;
    bool clampBySurfaceArea = true;
};

struct ShadowExclusionVolume : RuntimeVolume {
    bool excludeStaticShadows = true;
    bool excludeDynamicShadows = true;
    bool affectVolumetricShadows = false;
    float fadeDistance = 0.5f;
};

struct CullingDistanceVolume : RuntimeVolume {
    float nearDistance = 0.0f;
    float farDistance = 128.0f;
    bool applyToStaticObjects = true;
    bool applyToDynamicObjects = true;
    bool allowHlod = true;
};

struct ReferenceImagePlane : RuntimeVolume {
    std::string textureId;
    std::string imageUrl;
    ri::math::Vec3 tintColor{1.0f, 1.0f, 1.0f};
    float opacity = 0.88f;
    int renderOrder = 60;
    bool alwaysFaceCamera = false;
};

struct Text3dPrimitive : RuntimeVolume {
    std::string text = "TEXT";
    std::string fontFamily = "default";
    std::string materialId;
    std::string textColor = "#ffffff";
    std::string outlineColor = "#000000";
    float textScale = 1.0f;
    float depth = 0.08f;
    float extrusionBevel = 0.02f;
    float letterSpacing = 0.0f;
    bool alwaysFaceCamera = false;
    bool doubleSided = true;
};

struct AnnotationCommentPrimitive : RuntimeVolume {
    std::string text = "NOTE";
    std::string accentColor = "#ffd36a";
    std::string backgroundColor = "rgba(26, 22, 16, 0.88)";
    float textScale = 2.4f;
    float fontSize = 24.0f;
    bool alwaysFaceCamera = false;
};

enum class MeasureToolMode {
    Box,
    Line,
};

struct MeasureToolPrimitive : RuntimeVolume {
    MeasureToolMode mode = MeasureToolMode::Box;
    ri::math::Vec3 lineStart{};
    ri::math::Vec3 lineEnd{1.0f, 0.0f, 0.0f};
    ri::math::Vec3 labelOffset{0.0f, 0.8f, 0.0f};
    std::string unitSuffix = "u";
    std::string accentColor = "#8cd8ff";
    std::string backgroundColor = "rgba(7, 18, 28, 0.82)";
    std::string textColor = "#eaf8ff";
    float textScale = 3.2f;
    float fontSize = 34.0f;
    bool showWireframe = true;
    bool showFill = true;
    bool alwaysFaceCamera = true;
};

struct MeasureToolReadout {
    std::string id;
    MeasureToolMode mode = MeasureToolMode::Box;
    std::string label;
    ri::math::Vec3 labelPosition{};
    double primaryValue = 0.0;
    ri::math::Vec3 dimensions{};
    bool showWireframe = true;
    bool showFill = true;
};

struct RenderTargetSurface : RuntimeVolume {
    ri::math::Vec3 cameraPosition{0.0f, 2.0f, 0.0f};
    ri::math::Vec3 cameraLookAt{0.0f, 2.0f, -4.0f};
    float cameraFovDegrees = 55.0f;
    int renderResolution = 256;
    int resolutionCap = 512;
    float maxActiveDistance = 20.0f;
    std::uint32_t updateEveryFrames = 1;
    bool enableDistanceGate = true;
    bool editorOnly = false;
};

struct PlanarReflectionSurface : RuntimeVolume {
    ri::math::Vec3 planeNormal{0.0f, 0.0f, 1.0f};
    float reflectionStrength = 1.0f;
    int renderResolution = 256;
    int resolutionCap = 512;
    float maxActiveDistance = 18.0f;
    std::uint32_t updateEveryFrames = 1;
    bool enableDistanceGate = true;
    bool editorOnly = false;
};

enum class DynamicSurfaceKind {
    RenderTarget,
    PlanarReflection,
};

struct DynamicSurfaceRenderState {
    std::string id;
    DynamicSurfaceKind kind = DynamicSurfaceKind::RenderTarget;
    bool insideVolume = false;
    bool withinDistanceGate = true;
    bool shouldUpdate = false;
    int effectiveResolution = 256;
    std::uint32_t updateEveryFrames = 1;
    float viewerDistance = 0.0f;
};

enum class PassThroughPrimitiveShape {
    Box,
    Plane,
    Cylinder,
    Sphere,
    CustomMesh,
};

enum class PassThroughBlendMode {
    Alpha,
    Additive,
    Premultiplied,
};

struct PassThroughMaterialSettings {
    std::string baseColor = "#7fd6ff";
    float opacity = 0.35f;
    std::string emissiveColor = "#7fd6ff";
    float emissiveIntensity = 0.15f;
    bool doubleSided = true;
    bool depthWrite = false;
    bool depthTest = true;
    PassThroughBlendMode blendMode = PassThroughBlendMode::Alpha;
};

struct PassThroughVisualBehavior {
    bool pulseEnabled = false;
    float pulseSpeed = 1.2f;
    float pulseMinOpacity = 0.20f;
    float pulseMaxOpacity = 0.45f;
    bool distanceFadeEnabled = false;
    float fadeNear = 1.0f;
    float fadeFar = 20.0f;
    bool rimHighlightEnabled = false;
    float rimPower = 2.0f;
};

struct PassThroughInteractionProfile {
    bool blocksPlayer = false;
    bool blocksNpc = false;
    bool blocksProjectiles = false;
    bool affectsNavigation = false;
    bool raycastSelectable = true;
};

struct PassThroughEventHooks {
    std::string onEnter;
    std::string onExit;
    std::string onUse;
};

struct PassThroughDebugSettings {
    std::string label;
    bool showBounds = false;
};

struct PassThroughPrimitive : RuntimeVolume {
    PassThroughPrimitiveShape primitiveShape = PassThroughPrimitiveShape::Box;
    std::string customMeshAsset;
    PassThroughMaterialSettings material{};
    PassThroughVisualBehavior visualBehavior{};
    PassThroughInteractionProfile interactionProfile{};
    PassThroughEventHooks events{};
    PassThroughDebugSettings debug{};
    bool passThrough = true;
};

struct PassThroughVisualState {
    std::string id;
    float effectiveOpacity = 0.35f;
    bool hasPulse = false;
    bool hasDistanceFade = false;
    bool hasConfiguredEvents = false;
    bool blocksAnyChannel = false;
    bool invisibleWallRisk = false;
};

enum class SkyProjectionVisualMode {
    Solid,
    Gradient,
    Texture,
};

enum class SkyProjectionRenderLayer {
    Background,
    World,
    Foreground,
};

struct SkyProjectionVisualSettings {
    SkyProjectionVisualMode mode = SkyProjectionVisualMode::Solid;
    std::string color = "#8ab4ff";
    std::string topColor = "#b8d4ff";
    std::string bottomColor = "#6f94cc";
    std::string textureId;
    float opacity = 1.0f;
    bool doubleSided = true;
    bool unlit = true;
};

struct SkyProjectionBehaviorSettings {
    bool followCameraYaw = false;
    float parallaxFactor = 0.0f;
    bool distanceLock = false;
    bool depthWrite = false;
    SkyProjectionRenderLayer renderLayer = SkyProjectionRenderLayer::Background;
};

struct SkyProjectionDebugSettings {
    std::string label;
    bool showBounds = false;
};

struct SkyProjectionSurface : RuntimeVolume {
    std::string primitiveType = "plane";
    SkyProjectionVisualSettings visual{};
    SkyProjectionBehaviorSettings behavior{};
    SkyProjectionDebugSettings debug{};
    bool skyProjectionSurface = true;
};

struct SkyProjectionSurfaceState {
    std::string id;
    ri::math::Vec3 projectedPosition{};
    float projectedYawRadians = 0.0f;
    float effectiveOpacity = 1.0f;
    bool usesTexture = false;
    bool backgroundLayer = true;
    bool requiresPerFrameUpdate = false;
};

enum class VolumetricEmitterSpawnMode {
    Uniform,
    Surface,
    NoiseClustered,
};

enum class VolumetricEmitterBlendMode {
    Alpha,
    Additive,
};

enum class VolumetricEmitterSortMode {
    PerEmitter,
};

struct VolumetricEmitterEmissionSettings {
    std::uint32_t particleCount = 96U;
    VolumetricEmitterSpawnMode spawnMode = VolumetricEmitterSpawnMode::Uniform;
    float lifetimeMinSeconds = 2.0f;
    float lifetimeMaxSeconds = 6.0f;
    float spawnRatePerSecond = 0.0f;
    bool loop = true;
};

struct VolumetricEmitterParticleSettings {
    float size = 0.08f;
    float sizeJitter = 0.35f;
    std::string color = "#d9dce4";
    float opacity = 0.18f;
    ri::math::Vec3 velocity{0.0f, 0.04f, 0.0f};
    ri::math::Vec3 velocityJitter{0.02f, 0.03f, 0.02f};
    bool softFade = true;
};

struct VolumetricEmitterRenderSettings {
    VolumetricEmitterBlendMode blendMode = VolumetricEmitterBlendMode::Alpha;
    bool depthWrite = false;
    bool depthTest = true;
    bool billboard = true;
    VolumetricEmitterSortMode sortMode = VolumetricEmitterSortMode::PerEmitter;
};

struct VolumetricEmitterCullingSettings {
    float maxActiveDistance = 40.0f;
    bool frustumCulling = true;
    bool pauseWhenOffscreen = true;
};

struct VolumetricEmitterDebugSettings {
    bool showBounds = false;
    bool showSpawnPoints = false;
    std::string label;
};

/// Gameplay-facing policy for `particle_spawn_volume` (local VFX / dust / drips / steam).
/// Inline tuning remains on \ref VolumetricEmitterEmissionSettings / particle / render / culling.
struct ParticleSpawnActivationPolicy {
    /// Listener within this distance activates the emitter; `0` = use \ref VolumetricEmitterCullingSettings::maxActiveDistance only.
    float outerProximityRadius = 0.0f;
    /// When set, simulation/emission runs only while the listener is inside the inner volume shape.
    bool strictInnerVolumeOnly = false;
    /// Cheap ambient strips that stay on (VFX manager may still apply global budgets).
    bool alwaysOnAmbient = false;
};

struct ParticleSpawnEmissionPolicy {
    std::uint32_t burstCountOnEnter = 0U;
    bool oneShot = false;
};

struct ParticleSpawnBudgetPolicy {
    std::uint32_t maxOnScreenCostHint = 0U;
    /// Disable entirely when engine quality tier is at or below this value (`0` = never quality-disabled).
    std::uint8_t disableAtOrBelowQualityTier = 0U;
};

struct ParticleSpawnBindingPolicy {
    /// Empty = world-fixed emission in volume; otherwise follow this scene node.
    std::string followNodeId;
    std::string followSocketName;
};

struct ParticleSpawnEnvironmentPolicy {
    bool applyGlobalWind = false;
    std::string localWindFieldVolumeId;
    bool reduceWhenOccluded = false;
    bool reduceWhenIndoor = false;
};

struct ParticleSpawnAuthoring {
    std::string displayName;
    /// Baked preset / system asset; inline curves stay under `emission` / `particle` on the volume.
    std::string particleSystemPresetId;
    std::string meshAssetId;
    std::string materialAssetId;
    bool worldCollision = false;
    ParticleSpawnActivationPolicy activation{};
    ParticleSpawnEmissionPolicy emissionPolicy{};
    ParticleSpawnBudgetPolicy budget{};
    ParticleSpawnBindingPolicy binding{};
    ParticleSpawnEnvironmentPolicy environment{};
};

struct VolumetricEmitterBounds : RuntimeVolume {
    VolumetricEmitterEmissionSettings emission{};
    VolumetricEmitterParticleSettings particle{};
    VolumetricEmitterRenderSettings render{};
    VolumetricEmitterCullingSettings culling{};
    VolumetricEmitterDebugSettings debug{};
    /// Set for authored `particle_spawn_volume` nodes; omitted for legacy `volumetric_emitter_bounds`.
    std::optional<ParticleSpawnAuthoring> particleSpawn;
};

struct VolumetricEmitterRuntimeState {
    std::string id;
    bool shouldSimulate = true;
    bool withinDistanceGate = true;
    bool insideVolume = false;
    float viewerDistance = 0.0f;
    std::uint32_t effectiveParticleCount = 96U;
    float spawnRatePerSecond = 0.0f;
    bool loop = true;
    /// Effective outer range used for activation (proximity policy or culling distance).
    float effectiveProximityRadius = 0.0f;
    bool strictInnerVolumeOnly = false;
    bool alwaysOnAmbient = false;
    std::uint32_t burstOnEnterCount = 0U;
    bool oneShot = false;
};

struct SplinePathFollowerRuntimeState {
    std::string id;
    bool active = false;
    bool splineValid = false;
    bool loop = true;
    float speedUnitsPerSecond = 0.0f;
    float normalizedProgress = 0.0f;
    ri::math::Vec3 samplePosition{};
    ri::math::Vec3 sampleForward{0.0f, 0.0f, 1.0f};
};

struct SplinePathFollowerPrimitive : RuntimeVolume {
    std::vector<ri::math::Vec3> splinePoints{};
    float speedUnitsPerSecond = 2.0f;
    bool loop = true;
    float phaseOffset = 0.0f;
};

struct CablePrimitive : RuntimeVolume {
    ri::math::Vec3 start{};
    ri::math::Vec3 end{0.0f, -2.0f, 0.0f};
    float swayAmplitude = 0.12f;
    float swayFrequency = 0.8f;
    bool collisionEnabled = false;
};

struct CableRuntimeState {
    std::string id;
    bool active = false;
    bool collisionEnabled = false;
    ri::math::Vec3 resolvedStart{};
    ri::math::Vec3 resolvedEnd{};
    float swayOffset = 0.0f;
};

struct ClippingRuntimeVolume : RuntimeVolume {
    std::vector<std::string> modes{"visibility"};
    bool enabled = true;
};

struct FilteredCollisionRuntimeVolume : RuntimeVolume {
    std::vector<std::string> channels{"player"};
};

struct OcclusionPortalVolume : RuntimeVolume {
    bool closed = true;
};

struct PostProcessState {
    bool enabled = true;
    std::vector<std::string> activeVolumes;
    std::string label = "none";
    ri::math::Vec3 tintColor{1.0f, 1.0f, 1.0f};
    float tintStrength = 0.0f;
    float blurAmount = 0.0f;
    float noiseAmount = 0.0f;
    float scanlineAmount = 0.0f;
    float barrelDistortion = 0.0f;
    float chromaticAberration = 0.0f;
};

struct PhysicsVolumeModifiers {
    float gravityScale = 1.0f;
    float jumpScale = 1.0f;
    float drag = 0.0f;
    float buoyancy = 0.0f;
    ri::math::Vec3 flow{};
    std::vector<std::string> activeVolumes;
    std::vector<std::string> activeFluids;
    std::vector<std::string> activeSurfaceVelocity;
    std::vector<std::string> activeRadialForces;
};

struct PhysicsConstraintState {
    std::vector<ConstraintAxis> lockAxes;
};

struct WaterSurfaceState {
    const WaterSurfacePrimitive* surface = nullptr;
    bool inside = false;
    float surfaceY = 0.0f;
    float waveOffset = 0.0f;
};

struct KinematicMotionState {
    ri::math::Vec3 translationDelta{};
    ri::math::Vec3 rotationDeltaDegrees{};
    std::vector<std::string> activeTranslationPrimitives{};
    std::vector<std::string> activeRotationPrimitives{};
};

struct AudioOcclusionState {
    std::vector<std::string> activeVolumes;
    float dampening = 0.0f;
    float volumeScale = 1.0f;
};

struct AudioEnvironmentState {
    std::vector<std::string> activeVolumes;
    std::string label = "none";
    float reverbMix = 0.0f;
    float echoDelayMs = 0.0f;
    float echoFeedback = 0.0f;
    float dampening = 0.0f;
    float volumeScale = 1.0f;
    float playbackRate = 1.0f;
};

struct AmbientAudioVolume : RuntimeVolume {
    std::string audioPath;
    float baseVolume = 0.35f;
    float maxDistance = 8.0f;
    std::string label = "ambient_audio";
    std::vector<ri::math::Vec3> splinePoints;
};

struct AmbientAudioContribution {
    std::string id;
    std::string label;
    std::string audioPath;
    float desiredVolume = 0.0f;
    float distance = 0.0f;
    float normalizedFalloff = 0.0f;
};

struct AmbientAudioMixState {
    std::vector<AmbientAudioContribution> contributions;
    float combinedDesiredVolume = 0.0f;
    float topDesiredVolume = 0.0f;
    std::string topContributionId = "none";
    std::string topContributionLabel = "none";
};

struct TraversalLinkSelectionState {
    const TraversalLinkVolume* selected = nullptr;
    std::vector<const TraversalLinkVolume*> matches{};
};

struct NavmeshModifierAggregateState {
    std::vector<const NavmeshModifierVolume*> matches{};
    float traversalCostMultiplier = 1.0f;
    float maxTraversalCost = 0.0f;
    std::string dominantTag = "none";
};

struct PivotAnchorBindingState {
    const PivotAnchorPrimitive* anchor = nullptr;
    ri::math::Vec3 resolvedPosition{};
    ri::math::Vec3 resolvedForwardAxis{0.0f, 0.0f, 1.0f};
};

struct SymmetryMirrorResult {
    const SymmetryMirrorPlane* plane = nullptr;
    ri::math::Vec3 mirroredPosition{};
    ri::math::Vec3 mirroredForward{};
    float signedDistanceToPlane = 0.0f;
    bool mirrored = false;
};

struct AuthoringPlacementState {
    const PivotAnchorPrimitive* pivotAnchor = nullptr;
    const SymmetryMirrorPlane* mirrorPlane = nullptr;
    ri::math::Vec3 resolvedPosition{};
    ri::math::Vec3 resolvedForward{0.0f, 0.0f, 1.0f};
    bool mirrored = false;
    bool snappedToGrid = false;
};

struct TriggerRuntimeVolume : RuntimeVolume {
    /// When false, the volume does not enter/stay; if the subject was inside, an exit is synthesized once (logic `OnEndTouch`).
    bool armed = true;
    bool playerInside = false;
    double broadcastFrequency = 0.0;
    double nextBroadcastAt = 0.0;
    std::string onEnterEvent;
    std::string onStayEvent;
    std::string onExitEvent;
};

struct GenericTriggerVolume : TriggerRuntimeVolume {};

/// Mutable state for logic routes targeting a door id (\ref ri::logic::ports door inputs/outputs).
struct LogicDoorRuntimeState {
    bool open = false;
    bool locked = false;
    bool enabled = true;
};

/// Mutable state for logic routes targeting a spawner id (\ref ri::logic::ports spawner vocabulary).
struct LogicSpawnerRuntimeState {
    bool activeSpawn = false;
    bool enabled = true;
};

struct LevelSpawnerDefinition {
    std::string id;
    std::string entityId;
    ri::math::Vec3 position{};
    ri::math::Vec3 rotation{};
    bool enabledByDefault = true;
};

struct ActiveSpawnerState {
    std::string id;
    std::string entityId;
    bool activeSpawn = false;
    bool enabled = true;
};

struct LevelAction {
    std::string targetId;
    std::string inputName;
};

struct LevelActionGroup {
    std::string id;
    std::vector<LevelAction> actions{};
};

struct LevelTargetGroup {
    std::string id;
    std::vector<std::string> targetIds{};
};

struct LevelEvent {
    std::string id;
    std::vector<std::string> actionGroupIds{};
};

struct LevelSequence {
    std::string id;
    std::vector<std::string> actionGroupIds{};
    bool loop = false;
};

struct WorldStateCondition {
    std::vector<std::string> requiredFlags{};
    std::vector<std::string> forbiddenFlags{};
    std::unordered_map<std::string, double> minimumValues{};
    std::unordered_map<std::string, double> maximumValues{};
};

struct DamageTriggerVolume : TriggerRuntimeVolume {
    float damagePerSecond = 18.0f;
    bool killInstant = false;
    std::string label;
};

struct SpatialQueryVolume : TriggerRuntimeVolume {
    std::uint32_t filterMask = 0;
};

struct SpatialQueryMatchState {
    std::vector<const SpatialQueryVolume*> matches;
    std::uint32_t combinedFilterMask = 0;
    bool hasUnfilteredVolume = false;
};

struct StreamingLevelVolume : TriggerRuntimeVolume {
    std::string targetLevel;
};

struct CheckpointSpawnVolume : TriggerRuntimeVolume {
    std::string targetLevel;
    ri::math::Vec3 respawn{};
    ri::math::Vec3 respawnRotation{};
};

struct TeleportVolume : TriggerRuntimeVolume {
    std::string targetId;
    ri::math::Vec3 targetPosition{};
    ri::math::Vec3 targetRotation{};
    ri::math::Vec3 offset{};
};

struct LaunchVolume : TriggerRuntimeVolume {
    ri::math::Vec3 impulse{0.0f, 8.0f, 0.0f};
    bool affectPhysics = true;
};

/// Bit in \ref AnalyticsHeatmapVolume::sampleSubjectMask; primary pawn / player probe (ProtoEngine parity).
inline constexpr std::uint32_t kAnalyticsHeatmapSamplePlayer = 1u << 0;

/// Space-use analytics volume (dwell + visit counts). Containment uses \ref IsPointInsideVolume like other triggers.
/// Runtime counters: \ref entryCount (outside→inside transitions), \ref dwellSeconds (simulation time inside).
/// \ref TriggerRuntimeVolume::playerInside mirrors spec “isInside” for the last-updated subject (player when driven by
/// \ref UpdateTriggerVolumesAt). Axis-aligned \ref RuntimeVolume geometry unless a future OBB path is added.
/// Optional periodic stay: set inherited \ref TriggerRuntimeVolume::broadcastFrequency (seconds).
struct AnalyticsHeatmapVolume : TriggerRuntimeVolume {
    std::size_t entryCount = 0;
    double dwellSeconds = 0.0;
    /// Which subjects may increment this volume; host checks mask for non-player agents. Default: player only.
    std::uint32_t sampleSubjectMask = kAnalyticsHeatmapSamplePlayer;
    /// Editor / “show triggers”: host may draw a tinted hull when true.
    bool debugDraw = false;
};

enum class TriggerTransitionKind {
    Enter,
    Stay,
    Exit,
};

struct TriggerTransition {
    std::string volumeId;
    std::string volumeType;
    TriggerTransitionKind kind = TriggerTransitionKind::Enter;
};

struct StreamingLevelRequest {
    std::string volumeId;
    std::string targetLevel;
};

struct CheckpointSpawnRequest {
    std::string volumeId;
    std::string targetLevel;
    ri::math::Vec3 respawn{};
    ri::math::Vec3 respawnRotation{};
};

struct TeleportRequest {
    std::string volumeId;
    std::string targetId;
    ri::math::Vec3 targetPosition{};
    ri::math::Vec3 targetRotation{};
    ri::math::Vec3 offset{};
};

struct LaunchRequest {
    std::string volumeId;
    ri::math::Vec3 impulse{};
    bool affectPhysics = true;
};

struct DamageRequest {
    std::string volumeId;
    float damagePerSecond = 0.0f;
    bool killInstant = false;
    std::string label;
};

struct TriggerUpdateResult {
    std::vector<TriggerTransition> transitions;
    std::vector<StreamingLevelRequest> streamingRequests;
    std::vector<CheckpointSpawnRequest> checkpointRequests;
    std::vector<TeleportRequest> teleportRequests;
    std::vector<LaunchRequest> launchRequests;
    std::vector<DamageRequest> damageRequests;
    std::vector<std::string> analyticsEnteredVolumes;
};

enum class EnvironmentalTransitionKind {
    Enter,
    Exit,
};

struct EnvironmentalVolumeTransition {
    std::string volumeId;
    std::string volumeType;
    EnvironmentalTransitionKind kind = EnvironmentalTransitionKind::Enter;
};

struct EnvironmentalVolumeUpdateResult {
    std::vector<EnvironmentalVolumeTransition> transitions;
};

enum class InteractionTargetKind {
    None,
    Door,
    InfoPanel,
};

struct InteractionTargetOptions {
    float maxDistance = 3.0f;
    float overlapRadius = 1.25f;
    std::string actionLabel = "E";
};

struct InteractionTargetState {
    InteractionTargetKind kind = InteractionTargetKind::None;
    std::string targetId;
    std::string interactionHook;
    std::string promptText;
    float distance = 0.0f;
    bool inRay = false;
    bool inOverlap = false;
};

struct AudioRoutingState {
    std::string environmentLabel = "none";
    float ambientLayer = 0.0f;
    float chaseLayer = 0.0f;
    float endingLayer = 0.0f;
};

/// Row for JSON/CSV export (host supplies session/build/level ids; no PII in volume ids).
struct AnalyticsHeatmapExportRow {
    std::string levelId;
    std::string volumeId;
    std::uint64_t entryCount = 0;
    double dwellSeconds = 0.0;
    std::string sessionId;
    std::string buildId;
};

struct RuntimeEnvironmentState {
    PostProcessState postProcess;
    AudioEnvironmentState audio;
    PhysicsVolumeModifiers physics;
    PhysicsConstraintState constraints;
    WaterSurfaceState waterSurface;
    KinematicMotionState kinematicMotion;
};

struct CameraShakePresentationState {
    bool active = false;
    double shakeUntil = 0.0;
    float shakeAmount = 0.0f;
    ri::math::Vec3 shakeOffset{};
};

struct SpawnStabilizationState {
    bool pendingSpawnStabilization = false;
    ri::math::Vec3 anchor{};
    double settleUntil = 0.0;
};

struct SafeLightRuntimeState {
    std::string id;
    std::string groupId;
    ri::math::Vec3 position{};
    float radius = 6.0f;
    float intensity = 1.0f;
    bool enabled = true;
    bool safeZone = false;
};

struct SafeLightCoverageState {
    bool insideSafeLight = false;
    float combinedCoverage = 0.0f;
    float strongestCoverage = 0.0f;
    std::vector<std::string> activeSafeLightIds{};
};

struct RuntimeStatsOverlayMetrics {
    bool enabled = false;
    bool attached = false;
    bool visible = false;
    double frameTimeMs = 0.0;
    double framesPerSecond = 0.0;
};

struct RuntimeDiagnosticsHelper {
    std::string sourceId;
    std::string sourceType;
    RuntimeVolume volume{};
    std::optional<ri::math::Vec3> gizmoLineStart{};
    std::optional<ri::math::Vec3> gizmoLineEnd{};
};

struct RuntimeDiagnosticsSnapshot {
    bool visible = false;
    bool debugHelpersVisible = false;
    std::string debugHelpersRoot = "runtime.world";
    std::uint64_t revision = 0U;
    std::vector<RuntimeDiagnosticsHelper> helpers;
};

class RuntimeEnvironmentService;

class RuntimeDiagnosticsLayer {
public:
    void SetVisible(bool visible);
    void ToggleVisible();
    [[nodiscard]] bool IsVisible() const;
    void SetDebugHelpersVisible(bool visible);
    void ToggleDebugHelpersVisible();
    [[nodiscard]] bool DebugHelpersVisible() const;
    void SetDebugHelpersRoot(std::string root);
    [[nodiscard]] const std::string& GetDebugHelpersRoot() const;
    void Rebuild(const RuntimeEnvironmentService& environment);
    [[nodiscard]] RuntimeDiagnosticsSnapshot Snapshot() const;

private:
    bool visible_ = false;
    std::string debugHelpersRoot_ = "runtime.world";
    std::uint64_t revision_ = 0U;
    std::vector<RuntimeDiagnosticsHelper> helpers_{};
};

struct HelperActivityState {
    std::string lastAudioEvent = "none";
    std::string lastStateChange = "none";
    std::string lastTriggerEvent = "none";
    std::string lastEntityIoEvent = "none";
    std::string lastMessage = "none";
    std::string lastLevelEvent = "none";
    std::string lastSchemaEvent = "none";
};

struct RuntimeHelperMetricsSnapshot {
    std::size_t schemaValidations = 0;
    std::size_t schemaValidationFailures = 0;
    std::size_t tuningParses = 0;
    std::size_t tuningParseFailures = 0;
    std::size_t eventBusEmits = 0;
    std::size_t eventBusListeners = 0;
    std::size_t eventBusListenersAdded = 0;
    std::size_t eventBusListenersRemoved = 0;
    std::size_t audioManagedSounds = 0;
    std::size_t audioLoopsCreated = 0;
    std::size_t audioOneShotsPlayed = 0;
    std::size_t audioVoicesPlayed = 0;
    bool audioVoiceActive = false;
    std::size_t audioEnvironmentChanges = 0;
    std::string audioEnvironment = "none";
    double audioEnvironmentMix = 0.0;
    std::string runtimeSession = "session:none";
    std::string lastAudioEvent = "none";
    std::string lastStateChange = "none";
    std::string lastTriggerEvent = "none";
    std::string lastEntityIoEvent = "none";
    std::string lastMessage = "none";
    std::string lastLevelEvent = "none";
    std::string lastSchemaEvent = "none";
    std::size_t postProcessVolumes = 0;
    std::size_t audioReverbVolumes = 0;
    std::size_t audioOcclusionVolumes = 0;
    std::size_t ambientAudioVolumes = 0;
    std::size_t localizedFogVolumes = 0;
    std::size_t volumetricFogBlockers = 0;
    std::size_t fluidSimulationVolumes = 0;
    std::size_t physicsModifierVolumes = 0;
    std::size_t surfaceVelocityPrimitives = 0;
    std::size_t waterSurfacePrimitives = 0;
    std::size_t radialForceVolumes = 0;
    std::size_t physicsConstraintVolumes = 0;
    std::size_t kinematicTranslationPrimitives = 0;
    std::size_t kinematicRotationPrimitives = 0;
    std::size_t cameraBlockingVolumes = 0;
    std::size_t aiPerceptionBlockerVolumes = 0;
    std::size_t safeZoneVolumes = 0;
    std::size_t traversalLinkVolumes = 0;
    std::size_t localGridSnapVolumes = 0;
    std::size_t hintPartitionVolumes = 0;
    std::size_t doorWindowCutoutPrimitives = 0;
    std::size_t cameraConfinementVolumes = 0;
    std::size_t lodOverrideVolumes = 0;
    std::size_t lodSwitchPrimitives = 0;
    std::size_t lodSwitchSwitches = 0;
    std::size_t lodSwitchThrashWarnings = 0;
    std::size_t instanceCloudPrimitives = 0;
    std::size_t instanceCloudActiveInstances = 0;
    std::size_t surfaceScatterVolumes = 0;
    std::size_t surfaceScatterActiveInstances = 0;
    std::size_t splineMeshDeformers = 0;
    std::size_t splineMeshDeformerSegments = 0;
    std::size_t splineDecalRibbons = 0;
    std::size_t splineDecalRibbonTriangles = 0;
    std::size_t topologicalUvRemappers = 0;
    std::size_t triPlanarNodes = 0;
    std::size_t proceduralUvProjectionEstimatedPatches = 0;
    std::size_t voronoiFracturePrimitives = 0;
    std::size_t metaballPrimitives = 0;
    std::size_t latticeVolumes = 0;
    std::size_t manifoldSweepPrimitives = 0;
    std::size_t trimSheetSweepPrimitives = 0;
    std::size_t lSystemBranchPrimitives = 0;
    std::size_t geodesicSpherePrimitives = 0;
    std::size_t extrudeAlongNormalPrimitives = 0;
    std::size_t superellipsoidPrimitives = 0;
    std::size_t primitiveDemoLatticePrimitives = 0;
    std::size_t primitiveDemoVoronoiPrimitives = 0;
    std::size_t thickPolygonPrimitives = 0;
    std::size_t structuralProfilePrimitives = 0;
    std::size_t halfPipePrimitives = 0;
    std::size_t quarterPipePrimitives = 0;
    std::size_t pipeElbowPrimitives = 0;
    std::size_t torusSlicePrimitives = 0;
    std::size_t splineSweepPrimitives = 0;
    std::size_t revolvePrimitives = 0;
    std::size_t domeVaultPrimitives = 0;
    std::size_t loftPrimitives = 0;
    std::size_t navmeshModifierVolumes = 0;
    std::size_t reflectionProbeVolumes = 0;
    std::size_t lightImportanceVolumes = 0;
    std::size_t lightPortalVolumes = 0;
    std::size_t voxelGiBoundsVolumes = 0;
    std::size_t lightmapDensityVolumes = 0;
    std::size_t shadowExclusionVolumes = 0;
    std::size_t cullingDistanceVolumes = 0;
    std::size_t referenceImagePlanes = 0;
    std::size_t text3dPrimitives = 0;
    std::size_t annotationCommentPrimitives = 0;
    std::size_t measureToolPrimitives = 0;
    std::size_t renderTargetSurfaces = 0;
    std::size_t planarReflectionSurfaces = 0;
    std::size_t passThroughPrimitives = 0;
    std::size_t skyProjectionSurfaces = 0;
    std::size_t infoPanelSpawners = 0;
    std::size_t volumetricEmitterBounds = 0;
    std::size_t splinePathFollowerPrimitives = 0;
    std::size_t cablePrimitives = 0;
    std::size_t clippingVolumes = 0;
    std::size_t filteredCollisionVolumes = 0;
    std::size_t portalPrimitives = 0;
    std::size_t antiPortalPrimitives = 0;
    std::size_t occlusionPortalVolumes = 0;
    std::size_t closedOcclusionPortals = 0;
    std::size_t genericTriggerVolumes = 0;
    std::size_t spatialQueryVolumes = 0;
    std::size_t pivotAnchorPrimitives = 0;
    std::size_t symmetryMirrorPlanes = 0;
    std::size_t streamingLevelVolumes = 0;
    std::size_t checkpointSpawnVolumes = 0;
    std::size_t teleportVolumes = 0;
    std::size_t launchVolumes = 0;
    std::size_t analyticsHeatmapVolumes = 0;
    std::size_t structuralGraphNodes = 0;
    std::size_t structuralGraphEdges = 0;
    std::size_t structuralGraphCycles = 0;
    std::size_t structuralGraphUnresolvedDependencies = 0;
    std::size_t structuralGraphCompilePhase = 0;
    std::size_t structuralGraphRuntimePhase = 0;
    std::size_t structuralGraphPostBuildPhase = 0;
    std::size_t structuralGraphFramePhase = 0;
    std::size_t structuralDeferredOperations = 0;
    std::size_t structuralDeferredUnsupportedOperations = 0;
    std::string structuralDeferredHealth = "none";
    std::string structuralDeferredStatusLine = "none";
    std::string structuralDeferredSummary = "none";
    std::string activePostProcess = "none";
    double activePostProcessTint = 0.0;
    bool statsOverlayEnabled = false;
    bool statsOverlayAttached = false;
    double statsOverlayFrameTimeMs = 0.0;
    double statsOverlayFramesPerSecond = 0.0;
};

class RuntimeEnvironmentService {
public:
    void SetPostProcessVolumes(std::vector<PostProcessVolume> volumes);
    void SetAudioReverbVolumes(std::vector<AudioReverbVolume> volumes);
    void SetAudioOcclusionVolumes(std::vector<AudioOcclusionVolume> volumes);
    void SetAmbientAudioVolumes(std::vector<AmbientAudioVolume> volumes);
    void SetLocalizedFogVolumes(std::vector<LocalizedFogVolume> volumes);
    void SetFogBlockerVolumes(std::vector<FogBlockerVolume> volumes);
    void SetFluidSimulationVolumes(std::vector<FluidSimulationVolume> volumes);
    void SetPhysicsModifierVolumes(std::vector<PhysicsModifierVolume> volumes);
    void SetSurfaceVelocityPrimitives(std::vector<SurfaceVelocityPrimitive> volumes);
    void SetWaterSurfacePrimitives(std::vector<WaterSurfacePrimitive> primitives);
    void SetRadialForceVolumes(std::vector<RadialForceVolume> volumes);
    void SetPhysicsConstraintVolumes(std::vector<PhysicsConstraintVolume> volumes);
    void SetKinematicTranslationPrimitives(std::vector<KinematicTranslationPrimitive> primitives);
    void SetKinematicRotationPrimitives(std::vector<KinematicRotationPrimitive> primitives);
    void SetCameraBlockingVolumes(std::vector<CameraBlockingVolume> volumes);
    void SetAiPerceptionBlockerVolumes(std::vector<AiPerceptionBlockerVolume> volumes);
    void SetSafeZoneVolumes(std::vector<SafeZoneRuntimeVolume> volumes);
    void SetTraversalLinkVolumes(std::vector<TraversalLinkVolume> volumes);
    void SetPivotAnchorPrimitives(std::vector<PivotAnchorPrimitive> primitives);
    void SetSymmetryMirrorPlanes(std::vector<SymmetryMirrorPlane> planes);
    void SetLocalGridSnapVolumes(std::vector<LocalGridSnapVolume> volumes);
    void SetHintPartitionVolumes(std::vector<HintPartitionVolume> volumes);
    void SetDoorWindowCutoutPrimitives(std::vector<DoorWindowCutoutPrimitive> primitives);
    void SetProceduralDoorEntities(std::vector<ProceduralDoorEntity> doors);
    void SetCameraConfinementVolumes(std::vector<CameraConfinementVolume> volumes);
    void SetLodOverrideVolumes(std::vector<LodOverrideVolume> volumes);
    void SetLodSwitchPrimitives(std::vector<LodSwitchPrimitive> primitives);
    void SetSurfaceScatterVolumes(std::vector<SurfaceScatterVolume> volumes);
    void SetSplineMeshDeformerPrimitives(std::vector<SplineMeshDeformerPrimitive> primitives);
    void SetSplineDecalRibbonPrimitives(std::vector<SplineDecalRibbonPrimitive> primitives);
    void SetTopologicalUvRemapperVolumes(std::vector<TopologicalUvRemapperVolume> volumes);
    void SetTriPlanarNodes(std::vector<TriPlanarNode> nodes);
    void SetInstanceCloudPrimitives(std::vector<InstanceCloudPrimitive> primitives);
    void SetVoronoiFracturePrimitives(std::vector<VoronoiFracturePrimitive> primitives);
    void SetMetaballPrimitives(std::vector<MetaballPrimitive> primitives);
    void SetLatticeVolumes(std::vector<LatticeVolume> volumes);
    void SetManifoldSweepPrimitives(std::vector<ManifoldSweepPrimitive> primitives);
    void SetTrimSheetSweepPrimitives(std::vector<TrimSheetSweepPrimitive> primitives);
    void SetLSystemBranchPrimitives(std::vector<LSystemBranchPrimitive> primitives);
    void SetGeodesicSpherePrimitives(std::vector<GeodesicSpherePrimitive> primitives);
    void SetExtrudeAlongNormalPrimitives(std::vector<ExtrudeAlongNormalPrimitive> primitives);
    void SetSuperellipsoidPrimitives(std::vector<SuperellipsoidPrimitive> primitives);
    void SetPrimitiveDemoLatticePrimitives(std::vector<PrimitiveDemoLattice> primitives);
    void SetPrimitiveDemoVoronoiPrimitives(std::vector<PrimitiveDemoVoronoi> primitives);
    void SetThickPolygonPrimitives(std::vector<ThickPolygonPrimitive> primitives);
    void SetStructuralProfilePrimitives(std::vector<StructuralProfilePrimitive> primitives);
    void SetHalfPipePrimitives(std::vector<HalfPipePrimitive> primitives);
    void SetQuarterPipePrimitives(std::vector<QuarterPipePrimitive> primitives);
    void SetPipeElbowPrimitives(std::vector<PipeElbowPrimitive> primitives);
    void SetTorusSlicePrimitives(std::vector<TorusSlicePrimitive> primitives);
    void SetSplineSweepPrimitives(std::vector<SplineSweepPrimitive> primitives);
    void SetRevolvePrimitives(std::vector<RevolvePrimitive> primitives);
    void SetDomeVaultPrimitives(std::vector<DomeVaultPrimitive> primitives);
    void SetLoftPrimitives(std::vector<LoftPrimitive> primitives);
    void SetNavmeshModifierVolumes(std::vector<NavmeshModifierVolume> volumes);
    void SetReflectionProbeVolumes(std::vector<ReflectionProbeVolume> volumes);
    void SetLightImportanceVolumes(std::vector<LightImportanceVolume> volumes);
    void SetLightPortalVolumes(std::vector<LightPortalVolume> volumes);
    void SetVoxelGiBoundsVolumes(std::vector<VoxelGiBoundsVolume> volumes);
    void SetLightmapDensityVolumes(std::vector<LightmapDensityVolume> volumes);
    void SetShadowExclusionVolumes(std::vector<ShadowExclusionVolume> volumes);
    void SetCullingDistanceVolumes(std::vector<CullingDistanceVolume> volumes);
    void SetReferenceImagePlanes(std::vector<ReferenceImagePlane> planes);
    void SetText3dPrimitives(std::vector<Text3dPrimitive> textPrimitives);
    void SetAnnotationCommentPrimitives(std::vector<AnnotationCommentPrimitive> comments);
    void SetMeasureToolPrimitives(std::vector<MeasureToolPrimitive> tools);
    void SetRenderTargetSurfaces(std::vector<RenderTargetSurface> surfaces);
    void SetPlanarReflectionSurfaces(std::vector<PlanarReflectionSurface> surfaces);
    void SetPassThroughPrimitives(std::vector<PassThroughPrimitive> primitives);
    void SetSkyProjectionSurfaces(std::vector<SkyProjectionSurface> surfaces);
    void SetDynamicInfoPanelSpawners(std::vector<DynamicInfoPanelSpawner> spawners);
    void SetVolumetricEmitterBounds(std::vector<VolumetricEmitterBounds> volumes);
    void SetSplinePathFollowerPrimitives(std::vector<SplinePathFollowerPrimitive> primitives);
    void SetCablePrimitives(std::vector<CablePrimitive> primitives);
    void SetClippingVolumes(std::vector<ClippingRuntimeVolume> volumes);
    void SetFilteredCollisionVolumes(std::vector<FilteredCollisionRuntimeVolume> volumes);
    void SetVisibilityPrimitives(std::vector<VisibilityPrimitive> primitives);
    void SetOcclusionPortalVolumes(std::vector<OcclusionPortalVolume> volumes);
    void SetDamageVolumes(std::vector<DamageTriggerVolume> volumes);
    void SetGenericTriggerVolumes(std::vector<GenericTriggerVolume> volumes);
    void SetSpatialQueryVolumes(std::vector<SpatialQueryVolume> volumes);
    void SetStreamingLevelVolumes(std::vector<StreamingLevelVolume> volumes);
    void SetCheckpointSpawnVolumes(std::vector<CheckpointSpawnVolume> volumes);
    void SetTeleportVolumes(std::vector<TeleportVolume> volumes);
    void SetLaunchVolumes(std::vector<LaunchVolume> volumes);
    void SetAnalyticsHeatmapVolumes(std::vector<AnalyticsHeatmapVolume> volumes);
    void SetSpatialQueryTracker(SpatialQueryTracker* tracker);

    [[nodiscard]] const std::vector<PostProcessVolume>& GetPostProcessVolumes() const;
    [[nodiscard]] const std::vector<AudioReverbVolume>& GetAudioReverbVolumes() const;
    [[nodiscard]] const std::vector<AudioOcclusionVolume>& GetAudioOcclusionVolumes() const;
    [[nodiscard]] const std::vector<AmbientAudioVolume>& GetAmbientAudioVolumes() const;
    [[nodiscard]] const std::vector<LocalizedFogVolume>& GetLocalizedFogVolumes() const;
    [[nodiscard]] const std::vector<FogBlockerVolume>& GetFogBlockerVolumes() const;
    [[nodiscard]] const std::vector<FluidSimulationVolume>& GetFluidSimulationVolumes() const;
    [[nodiscard]] const std::vector<PhysicsModifierVolume>& GetPhysicsModifierVolumes() const;
    [[nodiscard]] const std::vector<SurfaceVelocityPrimitive>& GetSurfaceVelocityPrimitives() const;
    [[nodiscard]] const std::vector<WaterSurfacePrimitive>& GetWaterSurfacePrimitives() const;
    [[nodiscard]] const std::vector<RadialForceVolume>& GetRadialForceVolumes() const;
    [[nodiscard]] const std::vector<PhysicsConstraintVolume>& GetPhysicsConstraintVolumes() const;
    [[nodiscard]] const std::vector<KinematicTranslationPrimitive>& GetKinematicTranslationPrimitives() const;
    [[nodiscard]] const std::vector<KinematicRotationPrimitive>& GetKinematicRotationPrimitives() const;
    [[nodiscard]] const std::vector<CameraBlockingVolume>& GetCameraBlockingVolumes() const;
    [[nodiscard]] const std::vector<AiPerceptionBlockerVolume>& GetAiPerceptionBlockerVolumes() const;
    [[nodiscard]] const std::vector<SafeZoneRuntimeVolume>& GetSafeZoneVolumes() const;
    [[nodiscard]] const std::vector<TraversalLinkVolume>& GetTraversalLinkVolumes() const;
    [[nodiscard]] const std::vector<PivotAnchorPrimitive>& GetPivotAnchorPrimitives() const;
    [[nodiscard]] const std::vector<SymmetryMirrorPlane>& GetSymmetryMirrorPlanes() const;
    [[nodiscard]] const std::vector<LocalGridSnapVolume>& GetLocalGridSnapVolumes() const;
    [[nodiscard]] const std::vector<HintPartitionVolume>& GetHintPartitionVolumes() const;
    [[nodiscard]] const std::vector<DoorWindowCutoutPrimitive>& GetDoorWindowCutoutPrimitives() const;
    [[nodiscard]] const std::vector<ProceduralDoorEntity>& GetProceduralDoorEntities() const;
    [[nodiscard]] const std::vector<CameraConfinementVolume>& GetCameraConfinementVolumes() const;
    [[nodiscard]] const std::vector<LodOverrideVolume>& GetLodOverrideVolumes() const;
    [[nodiscard]] const std::vector<LodSwitchPrimitive>& GetLodSwitchPrimitives() const;
    [[nodiscard]] const std::vector<SurfaceScatterVolume>& GetSurfaceScatterVolumes() const;
    [[nodiscard]] const std::vector<SplineMeshDeformerPrimitive>& GetSplineMeshDeformerPrimitives() const;
    [[nodiscard]] const std::vector<SplineDecalRibbonPrimitive>& GetSplineDecalRibbonPrimitives() const;
    [[nodiscard]] const std::vector<TopologicalUvRemapperVolume>& GetTopologicalUvRemapperVolumes() const;
    [[nodiscard]] const std::vector<TriPlanarNode>& GetTriPlanarNodes() const;
    [[nodiscard]] const std::vector<InstanceCloudPrimitive>& GetInstanceCloudPrimitives() const;
    [[nodiscard]] const std::vector<VoronoiFracturePrimitive>& GetVoronoiFracturePrimitives() const;
    [[nodiscard]] const std::vector<MetaballPrimitive>& GetMetaballPrimitives() const;
    [[nodiscard]] const std::vector<LatticeVolume>& GetLatticeVolumes() const;
    [[nodiscard]] const std::vector<ManifoldSweepPrimitive>& GetManifoldSweepPrimitives() const;
    [[nodiscard]] const std::vector<TrimSheetSweepPrimitive>& GetTrimSheetSweepPrimitives() const;
    [[nodiscard]] const std::vector<LSystemBranchPrimitive>& GetLSystemBranchPrimitives() const;
    [[nodiscard]] const std::vector<GeodesicSpherePrimitive>& GetGeodesicSpherePrimitives() const;
    [[nodiscard]] const std::vector<ExtrudeAlongNormalPrimitive>& GetExtrudeAlongNormalPrimitives() const;
    [[nodiscard]] const std::vector<SuperellipsoidPrimitive>& GetSuperellipsoidPrimitives() const;
    [[nodiscard]] const std::vector<PrimitiveDemoLattice>& GetPrimitiveDemoLatticePrimitives() const;
    [[nodiscard]] const std::vector<PrimitiveDemoVoronoi>& GetPrimitiveDemoVoronoiPrimitives() const;
    [[nodiscard]] const std::vector<ThickPolygonPrimitive>& GetThickPolygonPrimitives() const;
    [[nodiscard]] const std::vector<StructuralProfilePrimitive>& GetStructuralProfilePrimitives() const;
    [[nodiscard]] const std::vector<HalfPipePrimitive>& GetHalfPipePrimitives() const;
    [[nodiscard]] const std::vector<QuarterPipePrimitive>& GetQuarterPipePrimitives() const;
    [[nodiscard]] const std::vector<PipeElbowPrimitive>& GetPipeElbowPrimitives() const;
    [[nodiscard]] const std::vector<TorusSlicePrimitive>& GetTorusSlicePrimitives() const;
    [[nodiscard]] const std::vector<SplineSweepPrimitive>& GetSplineSweepPrimitives() const;
    [[nodiscard]] const std::vector<RevolvePrimitive>& GetRevolvePrimitives() const;
    [[nodiscard]] const std::vector<DomeVaultPrimitive>& GetDomeVaultPrimitives() const;
    [[nodiscard]] const std::vector<LoftPrimitive>& GetLoftPrimitives() const;
    [[nodiscard]] const std::vector<NavmeshModifierVolume>& GetNavmeshModifierVolumes() const;
    [[nodiscard]] const std::vector<ReflectionProbeVolume>& GetReflectionProbeVolumes() const;
    [[nodiscard]] const std::vector<LightImportanceVolume>& GetLightImportanceVolumes() const;
    [[nodiscard]] const std::vector<LightPortalVolume>& GetLightPortalVolumes() const;
    [[nodiscard]] const std::vector<VoxelGiBoundsVolume>& GetVoxelGiBoundsVolumes() const;
    [[nodiscard]] const std::vector<LightmapDensityVolume>& GetLightmapDensityVolumes() const;
    [[nodiscard]] const std::vector<ShadowExclusionVolume>& GetShadowExclusionVolumes() const;
    [[nodiscard]] const std::vector<CullingDistanceVolume>& GetCullingDistanceVolumes() const;
    [[nodiscard]] const std::vector<ReferenceImagePlane>& GetReferenceImagePlanes() const;
    [[nodiscard]] const std::vector<Text3dPrimitive>& GetText3dPrimitives() const;
    [[nodiscard]] const std::vector<AnnotationCommentPrimitive>& GetAnnotationCommentPrimitives() const;
    [[nodiscard]] const std::vector<MeasureToolPrimitive>& GetMeasureToolPrimitives() const;
    [[nodiscard]] const std::vector<RenderTargetSurface>& GetRenderTargetSurfaces() const;
    [[nodiscard]] const std::vector<PlanarReflectionSurface>& GetPlanarReflectionSurfaces() const;
    [[nodiscard]] const std::vector<PassThroughPrimitive>& GetPassThroughPrimitives() const;
    [[nodiscard]] const std::vector<SkyProjectionSurface>& GetSkyProjectionSurfaces() const;
    [[nodiscard]] const std::vector<DynamicInfoPanelSpawner>& GetDynamicInfoPanelSpawners() const;
    [[nodiscard]] const std::vector<VolumetricEmitterBounds>& GetVolumetricEmitterBounds() const;
    [[nodiscard]] const std::vector<SplinePathFollowerPrimitive>& GetSplinePathFollowerPrimitives() const;
    [[nodiscard]] const std::vector<CablePrimitive>& GetCablePrimitives() const;
    [[nodiscard]] const std::vector<ClippingRuntimeVolume>& GetClippingVolumes() const;
    [[nodiscard]] const std::vector<FilteredCollisionRuntimeVolume>& GetFilteredCollisionVolumes() const;
    [[nodiscard]] const std::vector<VisibilityPrimitive>& GetVisibilityPrimitives() const;
    [[nodiscard]] const std::vector<OcclusionPortalVolume>& GetOcclusionPortalVolumes() const;
    [[nodiscard]] const std::vector<DamageTriggerVolume>& GetDamageVolumes() const;
    [[nodiscard]] const std::vector<GenericTriggerVolume>& GetGenericTriggerVolumes() const;
    [[nodiscard]] const std::vector<SpatialQueryVolume>& GetSpatialQueryVolumes() const;
    [[nodiscard]] const std::vector<StreamingLevelVolume>& GetStreamingLevelVolumes() const;
    [[nodiscard]] const std::vector<CheckpointSpawnVolume>& GetCheckpointSpawnVolumes() const;
    [[nodiscard]] const std::vector<TeleportVolume>& GetTeleportVolumes() const;
    [[nodiscard]] const std::vector<LaunchVolume>& GetLaunchVolumes() const;
    [[nodiscard]] const std::vector<AnalyticsHeatmapVolume>& GetAnalyticsHeatmapVolumes() const;
    [[nodiscard]] std::size_t CountVisibilityPrimitives(VisibilityPrimitiveKind kind) const;
    [[nodiscard]] std::size_t CountClosedOcclusionPortals() const;
    bool SetOcclusionPortalClosed(std::string_view portalId, bool closed);

    [[nodiscard]] PostProcessState GetActivePostProcessStateAt(const ri::math::Vec3& position) const;
    [[nodiscard]] AudioOcclusionState GetActiveAudioOcclusionStateAt(const ri::math::Vec3& position) const;
    [[nodiscard]] AudioEnvironmentState GetActiveAudioEnvironmentStateAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<AmbientAudioContribution> GetAmbientAudioContributionsAt(const ri::math::Vec3& position) const;
    [[nodiscard]] AmbientAudioMixState GetAmbientAudioMixStateAt(const ri::math::Vec3& position) const;
    [[nodiscard]] PhysicsVolumeModifiers GetPhysicsVolumeModifiersAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const WaterSurfacePrimitive*> GetWaterSurfacePrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const WaterSurfacePrimitive* GetWaterSurfacePrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] WaterSurfaceState GetWaterSurfaceStateAt(const ri::math::Vec3& position, double timeSeconds = 0.0) const;
    [[nodiscard]] PhysicsConstraintState GetPhysicsConstraintStateAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const KinematicTranslationPrimitive*> GetKinematicTranslationPrimitivesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const KinematicRotationPrimitive*> GetKinematicRotationPrimitivesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] KinematicMotionState ResolveKinematicMotionAt(const ri::math::Vec3& position, double timeSeconds) const;
    [[nodiscard]] bool IsCameraBlockedAt(const ri::math::Vec3& position, std::string_view traceTag = "camera") const;
    [[nodiscard]] AiPerceptionBlockerState GetAiPerceptionBlockerStateAt(const ri::math::Vec3& position) const;
    [[nodiscard]] SafeZoneState GetSafeZoneStateAt(const ri::math::Vec3& position) const;
    [[nodiscard]] TraversalLinkSelectionState GetTraversalLinksAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const TraversalLinkVolume* GetTraversalLinkAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const PivotAnchorPrimitive* GetPivotAnchorAt(const ri::math::Vec3& position) const;
    [[nodiscard]] PivotAnchorBindingState ResolvePivotAnchorBindingAt(const ri::math::Vec3& position,
                                                                      const ri::math::Vec3& fallbackForward = {0.0f, 0.0f, 1.0f}) const;
    [[nodiscard]] const SymmetryMirrorPlane* GetSymmetryMirrorPlaneAt(const ri::math::Vec3& position) const;
    [[nodiscard]] SymmetryMirrorResult ResolveSymmetryMirrorAt(const ri::math::Vec3& position,
                                                               const ri::math::Vec3& forwardDirection = {0.0f, 0.0f, 1.0f}) const;
    [[nodiscard]] AuthoringPlacementState ResolveAuthoringPlacementAt(const ri::math::Vec3& position,
                                                                      const ri::math::Vec3& forwardDirection = {0.0f, 0.0f, 1.0f}) const;
    [[nodiscard]] const LocalGridSnapVolume* GetLocalGridSnapAt(const ri::math::Vec3& position) const;
    [[nodiscard]] ri::math::Vec3 SnapPositionToLocalGrid(const ri::math::Vec3& position) const;
    [[nodiscard]] const HintPartitionVolume* GetHintPartitionVolumeAt(const ri::math::Vec3& position) const;
    [[nodiscard]] HintPartitionState GetHintPartitionStateAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const DoorWindowCutoutPrimitive* GetDoorWindowCutoutAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const ProceduralDoorEntity* GetProceduralDoorEntityAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const DynamicInfoPanelSpawner* GetDynamicInfoPanelSpawnerAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::string GetInfoPanelInteractionPromptAt(const ri::math::Vec3& position) const;
    [[nodiscard]] bool TryInteractWithProceduralDoor(std::string_view doorId,
                                                     bool hasAccess,
                                                     std::string* outFeedback = nullptr);
    [[nodiscard]] std::vector<DoorTransitionRequest> ConsumePendingDoorTransitions();
    void ApplyDoorTransitionMetadata(const DoorTransitionRequest& request);
    [[nodiscard]] const CameraConfinementVolume* GetCameraConfinementVolumeAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const LodOverrideVolume*> GetLodOverridesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] LodOverrideSelectionState ResolveLodOverrideAt(const ri::math::Vec3& position,
                                                                 std::string_view targetId = {}) const;
    void UpdateLodSwitchPrimitives(const ri::math::Vec3& viewerPosition, double timeSeconds, float screenSizeMetric = 0.0f);
    [[nodiscard]] std::vector<LodSwitchSelectionState> GetLodSwitchSelectionStates() const;
    [[nodiscard]] std::vector<const SurfaceScatterVolume*> GetSurfaceScatterVolumesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const SurfaceScatterVolume* GetSurfaceScatterVolumeAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<SurfaceScatterRuntimeState> GetSurfaceScatterRuntimeStates(
        const ri::math::Vec3& viewerPosition) const;
    [[nodiscard]] std::vector<const SplineMeshDeformerPrimitive*> GetSplineMeshDeformerPrimitivesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] const SplineMeshDeformerPrimitive* GetSplineMeshDeformerPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<SplineMeshDeformerRuntimeState> GetSplineMeshDeformerRuntimeStates(
        const ri::math::Vec3& viewerPosition) const;
    [[nodiscard]] std::vector<const SplineDecalRibbonPrimitive*> GetSplineDecalRibbonPrimitivesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] const SplineDecalRibbonPrimitive* GetSplineDecalRibbonPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<SplineDecalRibbonRuntimeState> GetSplineDecalRibbonRuntimeStates(
        const ri::math::Vec3& viewerPosition) const;
    [[nodiscard]] std::vector<const TopologicalUvRemapperVolume*> GetTopologicalUvRemapperVolumesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] const TopologicalUvRemapperVolume* GetTopologicalUvRemapperVolumeAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const TriPlanarNode*> GetTriPlanarNodesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const TriPlanarNode* GetTriPlanarNodeAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<ProceduralUvProjectionRuntimeState> GetProceduralUvProjectionRuntimeStates(
        const ri::math::Vec3& viewerPosition) const;
    [[nodiscard]] std::vector<const InstanceCloudPrimitive*> GetInstanceCloudPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const InstanceCloudPrimitive* GetInstanceCloudPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<InstanceCloudRuntimeState> GetInstanceCloudRuntimeStates(
        const ri::math::Vec3& viewerPosition) const;
    [[nodiscard]] std::vector<const VoronoiFracturePrimitive*> GetVoronoiFracturePrimitivesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] const VoronoiFracturePrimitive* GetVoronoiFracturePrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const MetaballPrimitive*> GetMetaballPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const MetaballPrimitive* GetMetaballPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const LatticeVolume*> GetLatticeVolumesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const LatticeVolume* GetLatticeVolumeAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const ManifoldSweepPrimitive*> GetManifoldSweepPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const ManifoldSweepPrimitive* GetManifoldSweepPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const TrimSheetSweepPrimitive*> GetTrimSheetSweepPrimitivesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] const TrimSheetSweepPrimitive* GetTrimSheetSweepPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const LSystemBranchPrimitive*> GetLSystemBranchPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const LSystemBranchPrimitive* GetLSystemBranchPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const GeodesicSpherePrimitive*> GetGeodesicSpherePrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const GeodesicSpherePrimitive* GetGeodesicSpherePrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const ExtrudeAlongNormalPrimitive*> GetExtrudeAlongNormalPrimitivesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] const ExtrudeAlongNormalPrimitive* GetExtrudeAlongNormalPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const SuperellipsoidPrimitive*> GetSuperellipsoidPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const SuperellipsoidPrimitive* GetSuperellipsoidPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const PrimitiveDemoLattice*> GetPrimitiveDemoLatticePrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const PrimitiveDemoLattice* GetPrimitiveDemoLatticePrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const PrimitiveDemoVoronoi*> GetPrimitiveDemoVoronoiPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const PrimitiveDemoVoronoi* GetPrimitiveDemoVoronoiPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const ThickPolygonPrimitive*> GetThickPolygonPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const ThickPolygonPrimitive* GetThickPolygonPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const StructuralProfilePrimitive*> GetStructuralProfilePrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const StructuralProfilePrimitive* GetStructuralProfilePrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const HalfPipePrimitive*> GetHalfPipePrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const HalfPipePrimitive* GetHalfPipePrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const QuarterPipePrimitive*> GetQuarterPipePrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const QuarterPipePrimitive* GetQuarterPipePrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const PipeElbowPrimitive*> GetPipeElbowPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const PipeElbowPrimitive* GetPipeElbowPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const TorusSlicePrimitive*> GetTorusSlicePrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const TorusSlicePrimitive* GetTorusSlicePrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const SplineSweepPrimitive*> GetSplineSweepPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const SplineSweepPrimitive* GetSplineSweepPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const RevolvePrimitive*> GetRevolvePrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const RevolvePrimitive* GetRevolvePrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const DomeVaultPrimitive*> GetDomeVaultPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const DomeVaultPrimitive* GetDomeVaultPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const LoftPrimitive*> GetLoftPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const LoftPrimitive* GetLoftPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const NavmeshModifierVolume*> GetNavmeshModifiersAt(const ri::math::Vec3& position) const;
    [[nodiscard]] NavmeshModifierAggregateState GetNavmeshModifierAggregateAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const ReferenceImagePlane*> GetReferenceImagePlanesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const ReferenceImagePlane* GetReferenceImagePlaneAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const Text3dPrimitive*> GetText3dPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const Text3dPrimitive* GetText3dPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const AnnotationCommentPrimitive*> GetAnnotationCommentPrimitivesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] const AnnotationCommentPrimitive* GetAnnotationCommentPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const MeasureToolPrimitive*> GetMeasureToolPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const MeasureToolPrimitive* GetMeasureToolPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<MeasureToolReadout> GetMeasureToolReadoutsAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const RenderTargetSurface*> GetRenderTargetSurfacesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const RenderTargetSurface* GetRenderTargetSurfaceAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const PlanarReflectionSurface*> GetPlanarReflectionSurfacesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] const PlanarReflectionSurface* GetPlanarReflectionSurfaceAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<DynamicSurfaceRenderState> GetDynamicSurfaceRenderStates(
        const ri::math::Vec3& viewerPosition,
        std::uint64_t frameIndex) const;
    [[nodiscard]] std::vector<const PassThroughPrimitive*> GetPassThroughPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const PassThroughPrimitive* GetPassThroughPrimitiveAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<PassThroughVisualState> GetPassThroughVisualStates(
        const ri::math::Vec3& viewerPosition,
        double timeSeconds) const;
    [[nodiscard]] std::vector<const SkyProjectionSurface*> GetSkyProjectionSurfacesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const SkyProjectionSurface* GetSkyProjectionSurfaceAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<SkyProjectionSurfaceState> GetSkyProjectionSurfaceStates(
        const ri::math::Vec3& cameraPosition,
        float cameraYawRadians) const;
    [[nodiscard]] std::vector<const VolumetricEmitterBounds*> GetVolumetricEmitterBoundsAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] const VolumetricEmitterBounds* GetVolumetricEmitterBoundsAtPoint(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<VolumetricEmitterRuntimeState> GetVolumetricEmitterRuntimeStates(
        const ri::math::Vec3& viewerPosition) const;
    [[nodiscard]] std::vector<const SplinePathFollowerPrimitive*> GetSplinePathFollowerPrimitivesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<SplinePathFollowerRuntimeState> GetSplinePathFollowerRuntimeStates(
        const ri::math::Vec3& viewerPosition,
        double timeSeconds) const;
    [[nodiscard]] std::vector<const CablePrimitive*> GetCablePrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<CableRuntimeState> GetCableRuntimeStates(
        const ri::math::Vec3& viewerPosition,
        double timeSeconds) const;
    [[nodiscard]] std::vector<const ClippingRuntimeVolume*> GetClippingVolumesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const FilteredCollisionRuntimeVolume*> GetFilteredCollisionVolumesAt(
        const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const VisibilityPrimitive*> GetVisibilityPrimitivesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const GenericTriggerVolume*> GetGenericTriggerVolumesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] SpatialQueryMatchState GetSpatialQueryStateAt(const ri::math::Vec3& position,
                                                                std::uint32_t requiredFilterMask = 0U) const;
    [[nodiscard]] std::vector<const SpatialQueryVolume*> GetSpatialQueryVolumesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const StreamingLevelVolume* GetStreamingLevelVolumeAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const CheckpointSpawnVolume* GetCheckpointSpawnVolumeAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const TeleportVolume* GetTeleportVolumeAt(const ri::math::Vec3& position) const;
    [[nodiscard]] const LaunchVolume* GetLaunchVolumeAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::vector<const AnalyticsHeatmapVolume*> GetAnalyticsHeatmapVolumesAt(const ri::math::Vec3& position) const;
    [[nodiscard]] std::size_t GetTriggerIndexEntryCount() const;

    /// Logic-graph inputs `Enable` / `Disable` / `Toggle` (case-insensitive) for a \ref GenericTriggerVolume id.
    [[nodiscard]] bool ApplyGenericTriggerVolumeLogicInput(std::string_view volumeId, std::string_view inputName);

    void RegisterLogicDoor(std::string id, LogicDoorRuntimeState initial = {});
    void RegisterLogicSpawner(std::string id, LogicSpawnerRuntimeState initial = {});
    void ClearLogicWorldActors();
    [[nodiscard]] std::vector<std::string> GetRegisteredLogicDoorIds() const;
    [[nodiscard]] std::vector<std::string> GetRegisteredLogicSpawnerIds() const;

    /// Dispatches non-node `DispatchInput` targets: generic triggers, registered doors/spawners. Returns true if `actorId` matched a known actor.
    [[nodiscard]] bool ApplyWorldActorLogicInput(ri::logic::LogicGraph& graph,
                                                 std::string_view actorId,
                                                 std::string_view inputName,
                                                 const ri::logic::LogicContext& context);

    [[nodiscard]] TriggerUpdateResult UpdateTriggerVolumesAt(
        const ri::math::Vec3& position,
        double elapsedSeconds,
        ri::runtime::RuntimeEventBus* eventBus = nullptr,
        bool simulationTimeAdvances = true,
        ri::logic::LogicGraph* logicGraph = nullptr,
        const ri::logic::LogicContext* logicTriggerContext = nullptr);
    [[nodiscard]] EnvironmentalVolumeUpdateResult UpdateEnvironmentalVolumesAt(const ri::math::Vec3& position);
    [[nodiscard]] InteractionTargetState ResolveInteractionTarget(
        const ri::math::Vec3& origin,
        const ri::math::Vec3& forward,
        const InteractionTargetOptions& options = {}) const;
    [[nodiscard]] AudioRoutingState GetAudioRoutingStateAt(const ri::math::Vec3& position) const;
    /// Manual entry bump (e.g. scripted teleport inside). Respects \ref AnalyticsHeatmapVolume::sampleSubjectMask.
    std::size_t MarkAnalyticsHeatmapEntryAt(const ri::math::Vec3& position,
                                            std::uint32_t subjectMask = kAnalyticsHeatmapSamplePlayer);
    /// Adds dwell for volumes containing `position` when simulation time is advancing (not paused / not in menu).
    void AccumulateAnalyticsHeatmapTimeAt(const ri::math::Vec3& position,
                                          double deltaSeconds,
                                          bool simulationTimeAdvances = true,
                                          std::uint32_t subjectMask = kAnalyticsHeatmapSamplePlayer);
    /// Zeros entry/dwell and clears inside flags so the next touch counts as a fresh enter (level load / checkpoint).
    void ResetAnalyticsHeatmapStatistics();
    [[nodiscard]] RuntimeEnvironmentState GetActiveEnvironmentStateAt(const ri::math::Vec3& position,
                                                                      double timeSeconds = 0.0) const;
    void ArmCameraShake(double nowSeconds,
                        float amount,
                        const ri::math::Vec3& offset,
                        double durationSeconds = 0.25);
    void UpdatePresentationFeedback(double nowSeconds, double deltaSeconds);
    [[nodiscard]] CameraShakePresentationState GetCameraShakePresentationState(double nowSeconds) const;
    void ArmSpawnStabilization(const ri::math::Vec3& anchor, double nowSeconds, double settleSeconds = 0.25);
    [[nodiscard]] bool StabilizeFreshSpawnIfNeeded(double nowSeconds,
                                                   ri::math::Vec3& inOutPosition,
                                                   ri::math::Vec3* inOutVelocity = nullptr);
    [[nodiscard]] SpawnStabilizationState GetSpawnStabilizationState() const;
    void SetLevelSpawnerDefinitions(std::vector<LevelSpawnerDefinition> definitions);
    [[nodiscard]] const std::vector<LevelSpawnerDefinition>& GetLevelSpawnerDefinitions() const;
    [[nodiscard]] std::vector<ActiveSpawnerState> GetActiveSpawnerStates() const;
    void SetLevelActionGroups(std::vector<LevelActionGroup> groups);
    void SetLevelTargetGroups(std::vector<LevelTargetGroup> groups);
    void SetLevelEvents(std::vector<LevelEvent> events);
    void SetLevelSequences(std::vector<LevelSequence> sequences);
    [[nodiscard]] bool DispatchLevelEvent(ri::logic::LogicGraph& graph,
                                          std::string_view eventId,
                                          const ri::logic::LogicContext& context);
    [[nodiscard]] bool DispatchLevelSequenceStep(ri::logic::LogicGraph& graph,
                                                 std::string_view sequenceId,
                                                 std::size_t actionGroupIndex,
                                                 const ri::logic::LogicContext& context);
    void SetSafeLights(std::vector<SafeLightRuntimeState> lights);
    [[nodiscard]] const std::vector<SafeLightRuntimeState>& GetSafeLights() const;
    [[nodiscard]] SafeLightCoverageState GetSafeLightCoverageAt(const ri::math::Vec3& position) const;
    [[nodiscard]] bool SetLevelLightEnabled(std::string_view lightId, bool enabled);
    std::size_t SetLevelLightGroupEnabled(std::string_view groupId, bool enabled);
    void SetWorldFlag(std::string key, bool enabled = true);
    void ClearWorldFlag(std::string_view key);
    [[nodiscard]] bool HasWorldFlag(std::string_view key) const;
    void SetWorldValue(std::string key, double value);
    [[nodiscard]] double GetWorldValueOr(std::string_view key, double fallback = 0.0) const;
    [[nodiscard]] bool CheckWorldStateCondition(const WorldStateCondition& condition) const;

private:
    std::vector<PostProcessVolume> postProcessVolumes_;
    std::vector<AudioReverbVolume> audioReverbVolumes_;
    std::vector<AudioOcclusionVolume> audioOcclusionVolumes_;
    std::vector<AmbientAudioVolume> ambientAudioVolumes_;
    std::vector<LocalizedFogVolume> localizedFogVolumes_;
    std::vector<FogBlockerVolume> fogBlockerVolumes_;
    std::vector<FluidSimulationVolume> fluidSimulationVolumes_;
    std::vector<PhysicsModifierVolume> physicsModifierVolumes_;
    std::vector<SurfaceVelocityPrimitive> surfaceVelocityPrimitives_;
    std::vector<WaterSurfacePrimitive> waterSurfacePrimitives_;
    std::vector<RadialForceVolume> radialForceVolumes_;
    std::vector<PhysicsConstraintVolume> physicsConstraintVolumes_;
    std::vector<KinematicTranslationPrimitive> kinematicTranslationPrimitives_;
    std::vector<KinematicRotationPrimitive> kinematicRotationPrimitives_;
    std::vector<CameraBlockingVolume> cameraBlockingVolumes_;
    std::vector<AiPerceptionBlockerVolume> aiPerceptionBlockerVolumes_;
    std::vector<SafeZoneRuntimeVolume> safeZoneVolumes_;
    std::vector<TraversalLinkVolume> traversalLinkVolumes_;
    std::vector<PivotAnchorPrimitive> pivotAnchorPrimitives_;
    std::vector<SymmetryMirrorPlane> symmetryMirrorPlanes_;
    std::vector<LocalGridSnapVolume> localGridSnapVolumes_;
    std::vector<HintPartitionVolume> hintPartitionVolumes_;
    std::vector<DoorWindowCutoutPrimitive> doorWindowCutoutPrimitives_;
    std::vector<ProceduralDoorEntity> proceduralDoorEntities_;
    std::vector<CameraConfinementVolume> cameraConfinementVolumes_;
    std::vector<LodOverrideVolume> lodOverrideVolumes_;
    std::vector<LodSwitchPrimitive> lodSwitchPrimitives_;
    std::vector<SurfaceScatterVolume> surfaceScatterVolumes_;
    std::vector<SplineMeshDeformerPrimitive> splineMeshDeformerPrimitives_;
    std::vector<SplineDecalRibbonPrimitive> splineDecalRibbonPrimitives_;
    std::vector<TopologicalUvRemapperVolume> topologicalUvRemapperVolumes_;
    std::vector<TriPlanarNode> triPlanarNodes_;
    std::vector<InstanceCloudPrimitive> instanceCloudPrimitives_;
    std::vector<VoronoiFracturePrimitive> voronoiFracturePrimitives_;
    std::vector<MetaballPrimitive> metaballPrimitives_;
    std::vector<LatticeVolume> latticeVolumes_;
    std::vector<ManifoldSweepPrimitive> manifoldSweepPrimitives_;
    std::vector<TrimSheetSweepPrimitive> trimSheetSweepPrimitives_;
    std::vector<LSystemBranchPrimitive> lSystemBranchPrimitives_;
    std::vector<GeodesicSpherePrimitive> geodesicSpherePrimitives_;
    std::vector<ExtrudeAlongNormalPrimitive> extrudeAlongNormalPrimitives_;
    std::vector<SuperellipsoidPrimitive> superellipsoidPrimitives_;
    std::vector<PrimitiveDemoLattice> primitiveDemoLatticePrimitives_;
    std::vector<PrimitiveDemoVoronoi> primitiveDemoVoronoiPrimitives_;
    std::vector<ThickPolygonPrimitive> thickPolygonPrimitives_;
    std::vector<StructuralProfilePrimitive> structuralProfilePrimitives_;
    std::vector<HalfPipePrimitive> halfPipePrimitives_;
    std::vector<QuarterPipePrimitive> quarterPipePrimitives_;
    std::vector<PipeElbowPrimitive> pipeElbowPrimitives_;
    std::vector<TorusSlicePrimitive> torusSlicePrimitives_;
    std::vector<SplineSweepPrimitive> splineSweepPrimitives_;
    std::vector<RevolvePrimitive> revolvePrimitives_;
    std::vector<DomeVaultPrimitive> domeVaultPrimitives_;
    std::vector<LoftPrimitive> loftPrimitives_;
    std::vector<NavmeshModifierVolume> navmeshModifierVolumes_;
    std::vector<ReflectionProbeVolume> reflectionProbeVolumes_;
    std::vector<LightImportanceVolume> lightImportanceVolumes_;
    std::vector<LightPortalVolume> lightPortalVolumes_;
    std::vector<VoxelGiBoundsVolume> voxelGiBoundsVolumes_;
    std::vector<LightmapDensityVolume> lightmapDensityVolumes_;
    std::vector<ShadowExclusionVolume> shadowExclusionVolumes_;
    std::vector<CullingDistanceVolume> cullingDistanceVolumes_;
    std::vector<ReferenceImagePlane> referenceImagePlanes_;
    std::vector<Text3dPrimitive> text3dPrimitives_;
    std::vector<AnnotationCommentPrimitive> annotationCommentPrimitives_;
    std::vector<MeasureToolPrimitive> measureToolPrimitives_;
    std::vector<RenderTargetSurface> renderTargetSurfaces_;
    std::vector<PlanarReflectionSurface> planarReflectionSurfaces_;
    std::vector<PassThroughPrimitive> passThroughPrimitives_;
    std::vector<SkyProjectionSurface> skyProjectionSurfaces_;
    std::vector<DynamicInfoPanelSpawner> dynamicInfoPanelSpawners_;
    std::vector<VolumetricEmitterBounds> volumetricEmitterBounds_;
    std::vector<SplinePathFollowerPrimitive> splinePathFollowerPrimitives_;
    std::vector<CablePrimitive> cablePrimitives_;
    std::vector<ClippingRuntimeVolume> clippingVolumes_;
    std::vector<FilteredCollisionRuntimeVolume> filteredCollisionVolumes_;
    std::vector<VisibilityPrimitive> visibilityPrimitives_;
    std::vector<OcclusionPortalVolume> occlusionPortalVolumes_;
    std::vector<DamageTriggerVolume> damageVolumes_;
    std::vector<GenericTriggerVolume> genericTriggerVolumes_;
    std::vector<SpatialQueryVolume> spatialQueryVolumes_;
    std::vector<StreamingLevelVolume> streamingLevelVolumes_;
    std::vector<CheckpointSpawnVolume> checkpointSpawnVolumes_;
    std::vector<TeleportVolume> teleportVolumes_;
    std::vector<LaunchVolume> launchVolumes_;
    std::vector<AnalyticsHeatmapVolume> analyticsHeatmapVolumes_;
    std::vector<LevelSpawnerDefinition> levelSpawnerDefinitions_;
    std::vector<LevelActionGroup> levelActionGroups_;
    std::vector<LevelTargetGroup> levelTargetGroups_;
    std::vector<LevelEvent> levelEvents_;
    std::vector<LevelSequence> levelSequences_;
    std::vector<SafeLightRuntimeState> safeLights_;
    std::unordered_map<std::string, LogicDoorRuntimeState> logicDoorActors_;
    std::unordered_map<std::string, LogicSpawnerRuntimeState> logicSpawnerActors_;
    std::unordered_set<std::string> worldFlags_{};
    std::unordered_map<std::string, double> worldValues_{};
    double shakeUntil_ = 0.0;
    float shakeAmount_ = 0.0f;
    ri::math::Vec3 shakeOffset_{};
    bool pendingSpawnStabilization_ = false;
    ri::math::Vec3 spawnStabilizationAnchor_{};
    double spawnStabilizeUntil_ = 0.0;
    std::vector<DoorTransitionRequest> pendingDoorTransitions_{};
    std::unordered_map<std::string, bool> environmentalInsideById_{};
    mutable ri::spatial::BspSpatialIndex triggerIndex_{};
    mutable bool triggerIndexDirty_ = true;
    SpatialQueryTracker* spatialQueryTracker_ = nullptr;
    double lastTriggerUpdateSeconds_ = -1.0;

    void MarkTriggerIndexDirty();
    void RebuildTriggerIndexIfNeeded() const;
    [[nodiscard]] std::vector<std::string> QueryTriggerVolumeKeysAt(const ri::math::Vec3& position) const;
};

[[nodiscard]] bool IsPointInsideVolume(const ri::math::Vec3& point, const RuntimeVolume& volume);
[[nodiscard]] std::vector<AnalyticsHeatmapExportRow> BuildAnalyticsHeatmapExportRows(
    const RuntimeEnvironmentService& environment,
    std::string_view levelId,
    std::string_view sessionId,
    std::string_view buildId);
[[nodiscard]] RuntimeHelperMetricsSnapshot BuildRuntimeHelperMetricsSnapshot(
    std::string_view runtimeSessionId,
    const ri::runtime::RuntimeEventBusMetrics& eventBusMetrics,
    const ri::validation::SchemaValidationMetrics& schemaMetrics,
    const ri::audio::AudioManagerMetrics& audioMetrics,
    const RuntimeStatsOverlayMetrics& statsMetrics,
    const HelperActivityState& activity,
    const RuntimeEnvironmentService& environmentService,
    const std::optional<ri::structural::StructuralGraphSummary>& structuralSummary,
    const PostProcessState& activePostProcessState,
    const std::optional<ri::structural::StructuralDeferredPipelineResult>& deferredPipelineResult = std::nullopt);
[[nodiscard]] ri::render::PostProcessParameters BuildPostProcessParameters(
    const PostProcessState& state,
    double timeSeconds = 0.0,
    float staticFadeAmount = 0.0f);
[[nodiscard]] PostProcessState SetPostProcessingEnabled(const PostProcessState& state, bool enabled);
[[nodiscard]] PostProcessState ApplyPostProcessPhase(const PostProcessState& state, std::string_view phase);

} // namespace ri::world
