#pragma once

#include "RawIron/Spatial/SpatialIndex.h"
#include "RawIron/World/RuntimeState.h"

#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ri::world {

enum class ClipVolumeMode {
    Physics,
    AI,
    Visibility,
};

enum class CollisionChannel {
    Player,
    Physics,
    Camera,
    Bullet,
    AI,
    Vehicle,
};

struct RuntimeVolumeSeed {
    std::string id;
    std::string type;
    std::optional<bool> debugVisible;
    std::optional<VolumeShape> shape;
    std::optional<ri::math::Vec3> position;
    std::optional<ri::math::Vec3> rotationRadians;
    std::optional<ri::math::Vec3> size;
    std::optional<float> radius;
    std::optional<float> height;
    /// When set, \ref CreateGenericTriggerVolume initializes \ref TriggerRuntimeVolume::armed (logic graph `Enable`/`Disable`).
    std::optional<bool> startArmed;
};

struct VolumeDefaults {
    std::string runtimeId = "volume";
    std::string type = "volume";
    VolumeShape shape = VolumeShape::Box;
    ri::math::Vec3 size{4.0f, 4.0f, 4.0f};
};

struct FilteredCollisionVolume : RuntimeVolume {
    std::vector<CollisionChannel> channels;
    std::vector<std::string> includeTags;
    std::vector<std::string> excludeTags;
    bool requireAllIncludeTags = false;
    bool allowUntaggedTrace = true;
    std::uint32_t channelMask = 0U;
};

struct InvisibleStructuralProxyVolume : RuntimeVolume {
    std::string sourceId;
    bool collisionEnabled = true;
    bool queryEnabled = true;
    bool logicEnabled = true;
    float inflation = 0.0f;
};

struct CollisionChannelResolveResult {
    std::vector<CollisionChannel> channels;
    std::vector<std::string> unknownTokens;
    bool usedDefault = false;
    std::uint32_t mask = 0U;
};

struct ClipRuntimeVolume : RuntimeVolume {
    std::vector<ClipVolumeMode> modes;
    bool enabled = true;
};

struct DamageVolume : RuntimeVolume {
    float damagePerSecond = 18.0f;
    bool killInstant = false;
    std::string label;
};

struct CameraModifierVolume : RuntimeVolume {
    float fov = 58.0f;
    float priority = 0.0f;
    float blendDistance = 2.0f;
    float shakeAmplitude = 0.0f;
    float shakeFrequency = 0.0f;
    ri::math::Vec3 cameraOffset{};
};

struct CameraModifierBlendState {
    bool active = false;
    float fov = 58.0f;
    float shakeAmplitude = 0.0f;
    float shakeFrequency = 0.0f;
    ri::math::Vec3 cameraOffset{};
    std::vector<std::string> activeVolumeIds{};
};

struct SafeZoneVolume : RuntimeVolume {
    bool dropAggro = true;
};

[[nodiscard]] std::string ToString(ClipVolumeMode mode);
[[nodiscard]] std::string ToString(CollisionChannel channel);
[[nodiscard]] std::string ToString(ConstraintAxis axis);
[[nodiscard]] std::string ToString(TraversalLinkKind kind);
[[nodiscard]] std::string ToString(HintPartitionMode mode);
[[nodiscard]] std::string ToString(ForcedLod forcedLod);
[[nodiscard]] std::string ToString(VisibilityPrimitiveKind kind);
[[nodiscard]] std::string ToString(TriggerTransitionKind kind);

[[nodiscard]] std::vector<ClipVolumeMode> ParseClipVolumeModes(const std::vector<std::string>& rawModes);
[[nodiscard]] std::vector<CollisionChannel> ParseCollisionChannels(const std::vector<std::string>& rawChannels);
[[nodiscard]] std::vector<ConstraintAxis> ParseConstraintAxes(const std::vector<std::string>& rawAxes);
[[nodiscard]] CollisionChannelResolveResult ResolveCollisionChannelAuthoring(
    const std::vector<std::string>& rawChannels,
    CollisionChannel defaultChannel = CollisionChannel::Player);
[[nodiscard]] std::uint32_t BuildCollisionChannelMask(const std::vector<CollisionChannel>& channels);
[[nodiscard]] std::vector<std::string> NormalizeTraceTags(const std::vector<std::string>& rawTags);

[[nodiscard]] bool TraceTagMatchesVolume(std::string_view traceTag, const FilteredCollisionVolume& volume);
[[nodiscard]] bool TraceAndVolumeTagsMatch(std::string_view traceTag,
                                           const std::vector<std::string>& traceTags,
                                           const FilteredCollisionVolume& volume);

[[nodiscard]] RuntimeVolume CreateRuntimeVolume(const RuntimeVolumeSeed& data,
                                                const VolumeDefaults& defaults = {});
[[nodiscard]] FilteredCollisionVolume CreateFilteredCollisionVolume(const RuntimeVolumeSeed& data,
                                                                   const std::vector<std::string>& rawChannels,
                                                                   const VolumeDefaults& defaults = {});
[[nodiscard]] InvisibleStructuralProxyVolume CreateInvisibleStructuralProxyVolume(
    const RuntimeVolumeSeed& data,
    std::string_view sourceId = {},
    float inflation = 0.0f,
    bool collisionEnabled = true,
    bool queryEnabled = true,
    bool logicEnabled = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] std::vector<ri::spatial::SpatialEntry> BuildInvisibleStructuralProxySpatialEntries(
    const std::vector<InvisibleStructuralProxyVolume>& proxies);
[[nodiscard]] ClipRuntimeVolume CreateClipRuntimeVolume(const RuntimeVolumeSeed& data,
                                                        const std::vector<std::string>& rawModes,
                                                        bool enabled = true,
                                                        const VolumeDefaults& defaults = {});
[[nodiscard]] DamageVolume CreateDamageVolume(const RuntimeVolumeSeed& data,
                                              float damagePerSecond = 18.0f,
                                              bool killInstant = false,
                                              std::string_view label = {},
                                              const VolumeDefaults& defaults = {});
[[nodiscard]] CameraModifierVolume CreateCameraModifierVolume(const RuntimeVolumeSeed& data,
                                                              float fov = 58.0f,
                                                              float priority = 0.0f,
                                                              float blendDistance = 2.0f,
                                                              float shakeAmplitude = 0.0f,
                                                              float shakeFrequency = 0.0f,
                                                              const ri::math::Vec3& cameraOffset = {},
                                                              const VolumeDefaults& defaults = {});
[[nodiscard]] SafeZoneVolume CreateSafeZoneVolume(const RuntimeVolumeSeed& data,
                                                  bool dropAggro = true,
                                                  const VolumeDefaults& defaults = {});
[[nodiscard]] PhysicsModifierVolume CreateCustomGravityVolume(
    const RuntimeVolumeSeed& data,
    float gravityScale = 0.4f,
    float jumpScale = 1.0f,
    float drag = 0.0f,
    float buoyancy = 0.0f,
    const ri::math::Vec3& flow = {},
    const VolumeDefaults& defaults = {});
[[nodiscard]] PhysicsModifierVolume CreateDirectionalWindVolume(
    const RuntimeVolumeSeed& data,
    float drag = 0.4f,
    const ri::math::Vec3& flow = {4.5f, 0.0f, 0.0f},
    const VolumeDefaults& defaults = {});
[[nodiscard]] PhysicsModifierVolume CreateBuoyancyVolume(
    const RuntimeVolumeSeed& data,
    float gravityScale = 0.8f,
    float jumpScale = 0.9f,
    float drag = 1.2f,
    float buoyancy = 0.85f,
    const ri::math::Vec3& flow = {},
    const VolumeDefaults& defaults = {});
[[nodiscard]] SurfaceVelocityPrimitive CreateSurfaceVelocityPrimitive(const RuntimeVolumeSeed& data,
                                                                     const ri::math::Vec3& flow = {},
                                                                     const VolumeDefaults& defaults = {});
[[nodiscard]] WaterSurfacePrimitive CreateWaterSurfacePrimitive(const RuntimeVolumeSeed& data,
                                                                float waveAmplitude = 0.08f,
                                                                float waveFrequency = 0.6f,
                                                                float flowSpeed = 0.0f,
                                                                bool blocksUnderwaterFog = false,
                                                                const VolumeDefaults& defaults = {});
[[nodiscard]] RadialForceVolume CreateRadialForceVolume(const RuntimeVolumeSeed& data,
                                                        float strength = 4.2f,
                                                        float falloff = 1.0f,
                                                        float innerRadius = 0.0f,
                                                        const VolumeDefaults& defaults = {});
[[nodiscard]] PhysicsConstraintVolume CreatePhysicsConstraintVolume(const RuntimeVolumeSeed& data,
                                                                   const std::vector<std::string>& rawLockAxes,
                                                                   const VolumeDefaults& defaults = {});
[[nodiscard]] SplinePathFollowerPrimitive CreateSplinePathFollowerPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<ri::math::Vec3> splinePoints = {},
    float speedUnitsPerSecond = 2.0f,
    bool loop = true,
    float phaseOffset = 0.0f,
    const VolumeDefaults& defaults = {});
[[nodiscard]] CablePrimitive CreateCablePrimitive(const RuntimeVolumeSeed& data,
                                                  const ri::math::Vec3& start = {},
                                                  const ri::math::Vec3& end = {0.0f, -2.0f, 0.0f},
                                                  float swayAmplitude = 0.12f,
                                                  float swayFrequency = 0.8f,
                                                  bool collisionEnabled = false,
                                                  const VolumeDefaults& defaults = {});
[[nodiscard]] ClippingRuntimeVolume CreateClippingRuntimeVolume(const RuntimeVolumeSeed& data,
                                                                std::vector<std::string> modes = {"visibility"},
                                                                bool enabled = true,
                                                                const VolumeDefaults& defaults = {});
[[nodiscard]] FilteredCollisionRuntimeVolume CreateFilteredCollisionRuntimeVolume(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> channels = {"player"},
    const VolumeDefaults& defaults = {});
[[nodiscard]] KinematicTranslationPrimitive CreateKinematicTranslationPrimitive(const RuntimeVolumeSeed& data,
                                                                                const ri::math::Vec3& axis = {1.0f, 0.0f, 0.0f},
                                                                                float distance = 2.0f,
                                                                                float cycleSeconds = 3.0f,
                                                                                bool pingPong = true,
                                                                                const VolumeDefaults& defaults = {});
[[nodiscard]] KinematicRotationPrimitive CreateKinematicRotationPrimitive(
    const RuntimeVolumeSeed& data,
    const ri::math::Vec3& axis = {0.0f, 1.0f, 0.0f},
    float angularSpeedDegreesPerSecond = 45.0f,
    float maxAngleDegrees = 360.0f,
    bool pingPong = false,
    const VolumeDefaults& defaults = {});
[[nodiscard]] TraversalLinkVolume CreateTraversalLinkVolume(const RuntimeVolumeSeed& data,
                                                            TraversalLinkKind kind = TraversalLinkKind::General,
                                                            float climbSpeed = 3.4f,
                                                            const VolumeDefaults& defaults = {});
[[nodiscard]] PivotAnchorPrimitive CreatePivotAnchorPrimitive(const RuntimeVolumeSeed& data,
                                                              std::string anchorId = {},
                                                              const ri::math::Vec3& forwardAxis = {0.0f, 0.0f, 1.0f},
                                                              bool alignToSurfaceNormal = false,
                                                              const VolumeDefaults& defaults = {});
[[nodiscard]] SymmetryMirrorPlane CreateSymmetryMirrorPlane(const RuntimeVolumeSeed& data,
                                                            const ri::math::Vec3& planeNormal = {1.0f, 0.0f, 0.0f},
                                                            float planeOffset = 0.0f,
                                                            bool keepOriginal = true,
                                                            bool snapToGrid = false,
                                                            const VolumeDefaults& defaults = {});
[[nodiscard]] LocalGridSnapVolume CreateLocalGridSnapVolume(const RuntimeVolumeSeed& data,
                                                            float snapSize = 0.5f,
                                                            bool snapX = true,
                                                            bool snapY = true,
                                                            bool snapZ = true,
                                                            int priority = 0,
                                                            const VolumeDefaults& defaults = {});
[[nodiscard]] HintPartitionVolume CreateHintPartitionVolume(const RuntimeVolumeSeed& data,
                                                            HintPartitionMode mode = HintPartitionMode::Hint,
                                                            const VolumeDefaults& defaults = {});
[[nodiscard]] DoorWindowCutoutPrimitive CreateDoorWindowCutoutPrimitive(const RuntimeVolumeSeed& data,
                                                                        float openingWidth = 2.0f,
                                                                        float openingHeight = 2.4f,
                                                                        float sillHeight = 0.0f,
                                                                        float lintelHeight = 2.4f,
                                                                        bool carveCollision = true,
                                                                        bool carveVisual = true,
                                                                        const VolumeDefaults& defaults = {});
[[nodiscard]] CameraConfinementVolume CreateCameraConfinementVolume(const RuntimeVolumeSeed& data,
                                                                    const VolumeDefaults& defaults = {});
[[nodiscard]] LodOverrideVolume CreateLodOverrideVolume(const RuntimeVolumeSeed& data,
                                                        std::vector<std::string> targetIds = {},
                                                        ForcedLod forcedLod = ForcedLod::Near,
                                                        const VolumeDefaults& defaults = {});
[[nodiscard]] LodSwitchPrimitive CreateLodSwitchPrimitive(const RuntimeVolumeSeed& data,
                                                          std::vector<LodSwitchLevel> levels = {},
                                                          const LodSwitchPolicy& policy = {},
                                                          const LodSwitchDebugSettings& debug = {},
                                                          const VolumeDefaults& defaults = {});
[[nodiscard]] SurfaceScatterVolume CreateSurfaceScatterVolume(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    const SurfaceScatterSourceRepresentation& sourceRepresentation = {},
    const SurfaceScatterDensityControls& density = {},
    const SurfaceScatterDistributionControls& distribution = {},
    SurfaceScatterCollisionPolicy collisionPolicy = SurfaceScatterCollisionPolicy::None,
    const SurfaceScatterCullingPolicy& culling = {},
    const SurfaceScatterAnimationSettings& animation = {},
    const VolumeDefaults& defaults = {});
[[nodiscard]] SplineMeshDeformerPrimitive CreateSplineMeshDeformerPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    std::vector<ri::math::Vec3> splinePoints = {},
    std::uint32_t sampleCount = 16U,
    std::uint32_t sectionCount = 1U,
    float segmentLength = 2.0f,
    float tangentSmoothing = 0.5f,
    bool keepSource = false,
    bool collisionEnabled = false,
    bool navInfluence = false,
    bool dynamicEnabled = false,
    std::uint32_t seed = 1337U,
    std::uint32_t maxSamples = 256U,
    float maxActiveDistance = 120.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] SplineDecalRibbonPrimitive CreateSplineDecalRibbonPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<ri::math::Vec3> splinePoints = {},
    float width = 1.0f,
    std::uint32_t tessellation = 32U,
    float offsetY = 0.03f,
    float uvScaleU = 1.0f,
    float uvScaleV = 1.0f,
    float tangentSmoothing = 0.5f,
    bool transparentBlend = true,
    bool depthWrite = false,
    bool collisionEnabled = false,
    bool navInfluence = false,
    bool dynamicEnabled = false,
    std::uint32_t seed = 1337U,
    std::uint32_t maxSamples = 256U,
    float maxActiveDistance = 120.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] TopologicalUvRemapperVolume CreateTopologicalUvRemapperVolume(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    std::string remapMode = "triplanar",
    std::string textureX = {},
    std::string textureY = {},
    std::string textureZ = {},
    std::string sharedTextureId = {},
    float projectionScale = 1.0f,
    float blendSharpness = 4.0f,
    const ri::math::Vec3& axisWeights = {1.0f, 1.0f, 1.0f},
    std::uint32_t maxMaterialPatches = 256U,
    float maxActiveDistance = 512.0f,
    bool frustumCulling = true,
    const ProceduralUvProjectionDebugControls& debug = {},
    const VolumeDefaults& defaults = {});
[[nodiscard]] TriPlanarNode CreateTriPlanarNode(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    std::string textureX = {},
    std::string textureY = {},
    std::string textureZ = {},
    std::string sharedTextureId = {},
    float projectionScale = 1.0f,
    float blendSharpness = 4.0f,
    const ri::math::Vec3& axisWeights = {1.0f, 1.0f, 1.0f},
    std::uint32_t maxMaterialPatches = 256U,
    bool objectSpaceAxes = false,
    float maxActiveDistance = 512.0f,
    bool frustumCulling = true,
    const ProceduralUvProjectionDebugControls& debug = {},
    const VolumeDefaults& defaults = {});
[[nodiscard]] InstanceCloudPrimitive CreateInstanceCloudPrimitive(
    const RuntimeVolumeSeed& data,
    const InstanceCloudSourceRepresentation& sourceRepresentation = {},
    std::uint32_t count = 1U,
    const ri::math::Vec3& offsetStep = {},
    const ri::math::Vec3& distributionExtents = {},
    std::uint32_t seed = 1337U,
    const InstanceCloudVariationRanges& variation = {},
    InstanceCloudCollisionPolicy collisionPolicy = InstanceCloudCollisionPolicy::None,
    const InstanceCloudCullingPolicy& culling = {},
    const VolumeDefaults& defaults = {});
[[nodiscard]] VoronoiFracturePrimitive CreateVoronoiFracturePrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    std::uint32_t cellCount = 16U,
    float noiseJitter = 0.1f,
    std::uint32_t seed = 1337U,
    bool capOpenFaces = true,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] MetaballPrimitive CreateMetaballPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<ri::math::Vec3> controlPoints = {},
    float isoLevel = 0.5f,
    float smoothing = 0.35f,
    std::uint32_t resolution = 24U,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] LatticeVolume CreateLatticeVolume(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    const ri::math::Vec3& cellSize = {0.5f, 0.5f, 0.5f},
    float beamRadius = 0.08f,
    std::uint32_t maxCells = 2048U,
    float maxActiveDistance = 120.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] ManifoldSweepPrimitive CreateManifoldSweepPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    std::vector<ri::math::Vec3> splinePoints = {},
    float profileRadius = 0.25f,
    std::uint32_t sampleCount = 32U,
    bool capEnds = true,
    float maxActiveDistance = 128.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] TrimSheetSweepPrimitive CreateTrimSheetSweepPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    std::vector<ri::math::Vec3> splinePoints = {},
    std::string trimSheetId = {},
    float uvTileU = 1.0f,
    float uvTileV = 1.0f,
    std::uint32_t tessellation = 24U,
    float maxActiveDistance = 128.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] LSystemBranchPrimitive CreateLSystemBranchPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    std::uint32_t iterations = 4U,
    float segmentLength = 0.5f,
    float branchAngleDegrees = 22.5f,
    std::uint32_t seed = 1337U,
    float maxActiveDistance = 120.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] GeodesicSpherePrimitive CreateGeodesicSpherePrimitive(
    const RuntimeVolumeSeed& data,
    std::uint32_t subdivisionLevel = 2U,
    float radiusScale = 1.0f,
    float maxActiveDistance = 120.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] ExtrudeAlongNormalPrimitive CreateExtrudeAlongNormalPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    float distance = 0.2f,
    std::uint32_t shellCount = 1U,
    bool capOpenEdges = true,
    float maxActiveDistance = 120.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] SuperellipsoidPrimitive CreateSuperellipsoidPrimitive(
    const RuntimeVolumeSeed& data,
    float exponentX = 2.0f,
    float exponentY = 2.0f,
    float exponentZ = 2.0f,
    std::uint32_t radialSegments = 24U,
    std::uint32_t rings = 16U,
    float maxActiveDistance = 120.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] PrimitiveDemoLattice CreatePrimitiveDemoLattice(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    const ri::math::Vec3& cellSize = {0.6f, 0.6f, 0.6f},
    std::uint32_t maxCells = 1024U,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] PrimitiveDemoVoronoi CreatePrimitiveDemoVoronoi(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    std::uint32_t cellCount = 12U,
    float jitter = 0.1f,
    std::uint32_t seed = 1337U,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] ThickPolygonPrimitive CreateThickPolygonPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<ri::math::Vec3> points = {},
    float thickness = 0.2f,
    bool capTop = true,
    bool capBottom = true,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] StructuralProfilePrimitive CreateStructuralProfilePrimitive(
    const RuntimeVolumeSeed& data,
    std::string profileId = {},
    float profileScale = 1.0f,
    std::uint32_t segmentCount = 16U,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] HalfPipePrimitive CreateHalfPipePrimitive(
    const RuntimeVolumeSeed& data,
    float radius = 2.0f,
    float length = 6.0f,
    std::uint32_t radialSegments = 16U,
    float wallThickness = 0.2f,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] QuarterPipePrimitive CreateQuarterPipePrimitive(
    const RuntimeVolumeSeed& data,
    float radius = 2.0f,
    float length = 6.0f,
    std::uint32_t radialSegments = 12U,
    float wallThickness = 0.2f,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] PipeElbowPrimitive CreatePipeElbowPrimitive(
    const RuntimeVolumeSeed& data,
    float radius = 1.0f,
    float bendDegrees = 90.0f,
    std::uint32_t radialSegments = 16U,
    std::uint32_t bendSegments = 12U,
    float wallThickness = 0.15f,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] TorusSlicePrimitive CreateTorusSlicePrimitive(
    const RuntimeVolumeSeed& data,
    float majorRadius = 2.0f,
    float minorRadius = 0.5f,
    float sweepDegrees = 180.0f,
    std::uint32_t radialSegments = 24U,
    std::uint32_t tubularSegments = 16U,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] SplineSweepPrimitive CreateSplineSweepPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<std::string> targetIds = {},
    std::vector<ri::math::Vec3> splinePoints = {},
    float profileRadius = 0.25f,
    std::uint32_t sampleCount = 32U,
    bool capEnds = true,
    float maxActiveDistance = 120.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] RevolvePrimitive CreateRevolvePrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<ri::math::Vec3> profilePoints = {},
    float sweepDegrees = 360.0f,
    std::uint32_t segmentCount = 24U,
    bool capEnds = false,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] DomeVaultPrimitive CreateDomeVaultPrimitive(
    const RuntimeVolumeSeed& data,
    float radius = 4.0f,
    float thickness = 0.25f,
    float heightRatio = 0.5f,
    std::uint32_t radialSegments = 24U,
    float maxActiveDistance = 96.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] LoftPrimitive CreateLoftPrimitive(
    const RuntimeVolumeSeed& data,
    std::vector<ri::math::Vec3> pathPoints = {},
    std::vector<ri::math::Vec3> profilePoints = {},
    std::uint32_t segmentCount = 24U,
    bool capEnds = true,
    float maxActiveDistance = 120.0f,
    bool frustumCulling = true,
    const VolumeDefaults& defaults = {});
[[nodiscard]] NavmeshModifierVolume CreateNavmeshModifierVolume(const RuntimeVolumeSeed& data,
                                                                float traversalCost = 1.5f,
                                                                std::string_view tag = "modified",
                                                                const VolumeDefaults& defaults = {});
[[nodiscard]] VisibilityPrimitive CreateVisibilityPrimitive(const RuntimeVolumeSeed& data,
                                                            VisibilityPrimitiveKind kind = VisibilityPrimitiveKind::Portal,
                                                            const VolumeDefaults& defaults = {});
[[nodiscard]] ReflectionProbeVolume CreateReflectionProbeVolume(const RuntimeVolumeSeed& data,
                                                                float intensity = 1.0f,
                                                                float blendDistance = 1.5f,
                                                                std::uint32_t captureResolution = 256U,
                                                                bool boxProjection = true,
                                                                bool dynamicCapture = false,
                                                                const VolumeDefaults& defaults = {});
[[nodiscard]] LightImportanceVolume CreateLightImportanceVolume(const RuntimeVolumeSeed& data,
                                                                bool probeGridBounds = false,
                                                                const VolumeDefaults& defaults = {});
[[nodiscard]] LightPortalVolume CreateLightPortalVolume(const RuntimeVolumeSeed& data,
                                                        float transmission = 1.0f,
                                                        float softness = 0.1f,
                                                        float priority = 0.0f,
                                                        bool twoSided = false,
                                                        const VolumeDefaults& defaults = {});
[[nodiscard]] VoxelGiBoundsVolume CreateVoxelGiBoundsVolume(const RuntimeVolumeSeed& data,
                                                            float voxelSize = 1.0f,
                                                            std::uint32_t cascadeCount = 1U,
                                                            bool updateDynamics = true,
                                                            const VolumeDefaults& defaults = {});
[[nodiscard]] LightmapDensityVolume CreateLightmapDensityVolume(const RuntimeVolumeSeed& data,
                                                                float texelsPerMeter = 256.0f,
                                                                float minimumTexelsPerMeter = 64.0f,
                                                                float maximumTexelsPerMeter = 1024.0f,
                                                                bool clampBySurfaceArea = true,
                                                                const VolumeDefaults& defaults = {});
[[nodiscard]] ShadowExclusionVolume CreateShadowExclusionVolume(const RuntimeVolumeSeed& data,
                                                                bool excludeStaticShadows = true,
                                                                bool excludeDynamicShadows = true,
                                                                bool affectVolumetricShadows = false,
                                                                float fadeDistance = 0.5f,
                                                                const VolumeDefaults& defaults = {});
[[nodiscard]] CullingDistanceVolume CreateCullingDistanceVolume(const RuntimeVolumeSeed& data,
                                                                float nearDistance = 0.0f,
                                                                float farDistance = 128.0f,
                                                                bool applyToStaticObjects = true,
                                                                bool applyToDynamicObjects = true,
                                                                bool allowHlod = true,
                                                                const VolumeDefaults& defaults = {});
[[nodiscard]] ReferenceImagePlane CreateReferenceImagePlane(const RuntimeVolumeSeed& data,
                                                            std::string textureId = {},
                                                            std::string imageUrl = {},
                                                            const ri::math::Vec3& tintColor = {1.0f, 1.0f, 1.0f},
                                                            float opacity = 0.88f,
                                                            int renderOrder = 60,
                                                            bool alwaysFaceCamera = false,
                                                            const VolumeDefaults& defaults = {});
[[nodiscard]] Text3dPrimitive CreateText3dPrimitive(const RuntimeVolumeSeed& data,
                                                    std::string text = "TEXT",
                                                    std::string fontFamily = "default",
                                                    std::string materialId = {},
                                                    std::string textColor = "#ffffff",
                                                    std::string outlineColor = "#000000",
                                                    float textScale = 1.0f,
                                                    float depth = 0.08f,
                                                    float extrusionBevel = 0.02f,
                                                    float letterSpacing = 0.0f,
                                                    bool alwaysFaceCamera = false,
                                                    bool doubleSided = true,
                                                    const VolumeDefaults& defaults = {});
[[nodiscard]] AnnotationCommentPrimitive CreateAnnotationCommentPrimitive(const RuntimeVolumeSeed& data,
                                                                          std::string text = "NOTE",
                                                                          std::string accentColor = "#ffd36a",
                                                                          std::string backgroundColor = "rgba(26, 22, 16, 0.88)",
                                                                          float textScale = 2.4f,
                                                                          float fontSize = 24.0f,
                                                                          bool alwaysFaceCamera = false,
                                                                          const VolumeDefaults& defaults = {});
[[nodiscard]] MeasureToolPrimitive CreateMeasureToolPrimitive(const RuntimeVolumeSeed& data,
                                                              MeasureToolMode mode = MeasureToolMode::Box,
                                                              const ri::math::Vec3& lineStart = {0.0f, 0.0f, 0.0f},
                                                              const ri::math::Vec3& lineEnd = {1.0f, 0.0f, 0.0f},
                                                              const ri::math::Vec3& labelOffset = {0.0f, 0.8f, 0.0f},
                                                              std::string unitSuffix = "u",
                                                              std::string accentColor = "#8cd8ff",
                                                              std::string backgroundColor = "rgba(7, 18, 28, 0.82)",
                                                              std::string textColor = "#eaf8ff",
                                                              float textScale = 3.2f,
                                                              float fontSize = 34.0f,
                                                              bool showWireframe = true,
                                                              bool showFill = true,
                                                              bool alwaysFaceCamera = true,
                                                              const VolumeDefaults& defaults = {});
[[nodiscard]] RenderTargetSurface CreateRenderTargetSurface(const RuntimeVolumeSeed& data,
                                                            const ri::math::Vec3& cameraPosition = {0.0f, 2.0f, 0.0f},
                                                            const ri::math::Vec3& cameraLookAt = {0.0f, 2.0f, -4.0f},
                                                            float cameraFovDegrees = 55.0f,
                                                            int renderResolution = 256,
                                                            int resolutionCap = 512,
                                                            float maxActiveDistance = 20.0f,
                                                            std::uint32_t updateEveryFrames = 1U,
                                                            bool enableDistanceGate = true,
                                                            bool editorOnly = false,
                                                            const VolumeDefaults& defaults = {});
[[nodiscard]] PlanarReflectionSurface CreatePlanarReflectionSurface(const RuntimeVolumeSeed& data,
                                                                    const ri::math::Vec3& planeNormal = {0.0f, 0.0f, 1.0f},
                                                                    float reflectionStrength = 1.0f,
                                                                    int renderResolution = 256,
                                                                    int resolutionCap = 512,
                                                                    float maxActiveDistance = 18.0f,
                                                                    std::uint32_t updateEveryFrames = 1U,
                                                                    bool enableDistanceGate = true,
                                                                    bool editorOnly = false,
                                                                    const VolumeDefaults& defaults = {});
[[nodiscard]] PassThroughPrimitive CreatePassThroughPrimitive(const RuntimeVolumeSeed& data,
                                                              PassThroughPrimitiveShape primitiveShape = PassThroughPrimitiveShape::Box,
                                                              std::string customMeshAsset = {},
                                                              const PassThroughMaterialSettings& material = {},
                                                              const PassThroughVisualBehavior& visualBehavior = {},
                                                              const PassThroughInteractionProfile& interactionProfile = {},
                                                              const PassThroughEventHooks& events = {},
                                                              const PassThroughDebugSettings& debug = {},
                                                              const VolumeDefaults& defaults = {});
[[nodiscard]] SkyProjectionSurface CreateSkyProjectionSurface(const RuntimeVolumeSeed& data,
                                                              std::string primitiveType = "plane",
                                                              const SkyProjectionVisualSettings& visual = {},
                                                              const SkyProjectionBehaviorSettings& behavior = {},
                                                              const SkyProjectionDebugSettings& debug = {},
                                                              const VolumeDefaults& defaults = {});
[[nodiscard]] VolumetricEmitterBounds CreateVolumetricEmitterBounds(
    const RuntimeVolumeSeed& data,
    const VolumetricEmitterEmissionSettings& emission = {},
    const VolumetricEmitterParticleSettings& particle = {},
    const VolumetricEmitterRenderSettings& render = {},
    const VolumetricEmitterCullingSettings& culling = {},
    const VolumetricEmitterDebugSettings& debug = {},
    const VolumeDefaults& defaults = {},
    const std::optional<ParticleSpawnAuthoring>& particleSpawn = std::nullopt);
[[nodiscard]] OcclusionPortalVolume CreateOcclusionPortalVolume(const RuntimeVolumeSeed& data,
                                                                bool closed = true,
                                                                const VolumeDefaults& defaults = {});
[[nodiscard]] PostProcessVolume CreatePostProcessVolume(const RuntimeVolumeSeed& data,
                                                        float tintStrength = 0.12f,
                                                        float blurAmount = 0.0012f,
                                                        float noiseAmount = 0.003f,
                                                        float scanlineAmount = 0.0015f,
                                                        float barrelDistortion = 0.003f,
                                                        float chromaticAberration = 0.00025f,
                                                        const VolumeDefaults& defaults = {});
[[nodiscard]] AudioReverbVolume CreateAudioReverbVolume(const RuntimeVolumeSeed& data,
                                                        float reverbMix = 0.55f,
                                                        float echoDelayMs = 160.0f,
                                                        float echoFeedback = 0.42f,
                                                        float dampening = 0.08f,
                                                        float volumeScale = 1.0f,
                                                        float playbackRate = 1.0f,
                                                        const VolumeDefaults& defaults = {});
[[nodiscard]] AudioOcclusionVolume CreateAudioOcclusionVolume(const RuntimeVolumeSeed& data,
                                                              float occlusionStrength = 0.45f,
                                                              float volumeScale = 0.78f,
                                                              const VolumeDefaults& defaults = {});
[[nodiscard]] AmbientAudioVolume CreateAmbientAudioVolume(const RuntimeVolumeSeed& data,
                                                          std::string audioPath = {},
                                                          float baseVolume = 0.35f,
                                                          float maxDistance = 8.0f,
                                                          std::string label = "ambient_audio",
                                                          std::vector<ri::math::Vec3> splinePoints = {},
                                                          const VolumeDefaults& defaults = {});
[[nodiscard]] GenericTriggerVolume CreateGenericTriggerVolume(const RuntimeVolumeSeed& data,
                                                              double broadcastFrequency = 0.0,
                                                              const VolumeDefaults& defaults = {});
[[nodiscard]] SpatialQueryVolume CreateSpatialQueryVolume(const RuntimeVolumeSeed& data,
                                                          double broadcastFrequency = 0.0,
                                                          std::uint32_t filterMask = 0,
                                                          const VolumeDefaults& defaults = {});
[[nodiscard]] StreamingLevelVolume CreateStreamingLevelVolume(const RuntimeVolumeSeed& data,
                                                              std::string targetLevel = {},
                                                              const VolumeDefaults& defaults = {});
[[nodiscard]] CheckpointSpawnVolume CreateCheckpointSpawnVolume(const RuntimeVolumeSeed& data,
                                                                std::string targetLevel = {},
                                                                const ri::math::Vec3& respawn = {},
                                                                const ri::math::Vec3& respawnRotation = {},
                                                                const VolumeDefaults& defaults = {});
[[nodiscard]] TeleportVolume CreateTeleportVolume(const RuntimeVolumeSeed& data,
                                                  std::string targetId = {},
                                                  const ri::math::Vec3& targetPosition = {},
                                                  const ri::math::Vec3& targetRotation = {},
                                                  const ri::math::Vec3& offset = {},
                                                  const VolumeDefaults& defaults = {});
[[nodiscard]] LaunchVolume CreateLaunchVolume(const RuntimeVolumeSeed& data,
                                              const ri::math::Vec3& impulse = {0.0f, 8.0f, 0.0f},
                                              bool affectPhysics = true,
                                              const VolumeDefaults& defaults = {});
[[nodiscard]] AnalyticsHeatmapVolume CreateAnalyticsHeatmapVolume(const RuntimeVolumeSeed& data,
                                                                  const VolumeDefaults& defaults = {});
[[nodiscard]] LocalizedFogVolume CreateLocalizedFogVolume(const RuntimeVolumeSeed& data,
                                                          float tintStrength = 0.12f,
                                                          float blurAmount = 0.0016f,
                                                          const VolumeDefaults& defaults = {});
[[nodiscard]] FogBlockerVolume CreateFogBlockerVolume(const RuntimeVolumeSeed& data,
                                                      const VolumeDefaults& defaults = {});
[[nodiscard]] FluidSimulationVolume CreateFluidSimulationVolume(const RuntimeVolumeSeed& data,
                                                                float gravityScale = 0.35f,
                                                                float jumpScale = 0.72f,
                                                                float drag = 1.8f,
                                                                float buoyancy = 0.9f,
                                                                const ri::math::Vec3& flow = {},
                                                                float tintStrength = 0.22f,
                                                                float reverbMix = 0.35f,
                                                                float echoDelayMs = 120.0f,
                                                                const VolumeDefaults& defaults = {});

[[nodiscard]] const CameraModifierVolume* GetActiveCameraModifierAt(
    const ri::math::Vec3& position,
    const std::vector<CameraModifierVolume>& volumes);
[[nodiscard]] CameraModifierBlendState BlendCameraModifierAt(
    const ri::math::Vec3& position,
    const std::vector<CameraModifierVolume>& volumes);
[[nodiscard]] bool IsPositionInSafeZone(const ri::math::Vec3& position,
                                        const std::vector<SafeZoneVolume>& volumes);

} // namespace ri::world
