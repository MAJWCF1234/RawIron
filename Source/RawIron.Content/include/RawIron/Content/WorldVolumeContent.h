#pragma once

#include "RawIron/Content/Value.h"
#include "RawIron/World/VolumeDescriptors.h"

namespace ri::content {

[[nodiscard]] ri::world::RuntimeVolumeSeed BuildRuntimeVolumeSeed(
    const Value::Object& data,
    const ri::world::VolumeDefaults& defaults = {});

[[nodiscard]] ri::world::AuthoringRuntimeVolumeRecord BuildAuthoringRuntimeVolumeRecordFromLevelObject(
    const Value::Object& data,
    const ri::world::VolumeDefaults& defaults = {});

[[nodiscard]] ri::world::FilteredCollisionVolume BuildFilteredCollisionVolume(const Value::Object& data);
[[nodiscard]] ri::world::FilteredCollisionVolume BuildCameraBlockingVolume(const Value::Object& data);
[[nodiscard]] ri::world::ClipRuntimeVolume BuildAiPerceptionBlockerVolume(const Value::Object& data);
[[nodiscard]] ri::world::DamageVolume BuildDamageVolume(const Value::Object& data, bool killInstant = false);
[[nodiscard]] ri::world::CameraModifierVolume BuildCameraModifierVolume(const Value::Object& data);
[[nodiscard]] ri::world::SafeZoneVolume BuildSafeZoneVolume(const Value::Object& data);
[[nodiscard]] ri::world::PhysicsModifierVolume BuildPhysicsVolume(const Value::Object& data);
[[nodiscard]] ri::world::PhysicsModifierVolume BuildCustomGravityVolume(const Value::Object& data);
[[nodiscard]] ri::world::PhysicsModifierVolume BuildDirectionalWindVolume(const Value::Object& data);
[[nodiscard]] ri::world::PhysicsModifierVolume BuildBuoyancyVolume(const Value::Object& data);
[[nodiscard]] ri::world::SurfaceVelocityPrimitive BuildSurfaceVelocityPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::WaterSurfacePrimitive BuildWaterSurfacePrimitive(const Value::Object& data);
[[nodiscard]] ri::world::RadialForceVolume BuildRadialForceVolume(const Value::Object& data);
[[nodiscard]] ri::world::PhysicsConstraintVolume BuildPhysicsConstraintVolume(const Value::Object& data);
[[nodiscard]] ri::world::SplinePathFollowerPrimitive BuildSplinePathFollowerPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::CablePrimitive BuildCablePrimitive(const Value::Object& data);
[[nodiscard]] ri::world::ClippingRuntimeVolume BuildClippingVolume(const Value::Object& data);
[[nodiscard]] ri::world::FilteredCollisionRuntimeVolume BuildFilteredCollisionRuntimeVolume(const Value::Object& data);
[[nodiscard]] ri::world::KinematicTranslationPrimitive BuildKinematicTranslationPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::KinematicRotationPrimitive BuildKinematicRotationPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::TraversalLinkVolume BuildTraversalLinkVolume(const Value::Object& data);
[[nodiscard]] ri::world::PivotAnchorPrimitive BuildPivotAnchorPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::SymmetryMirrorPlane BuildSymmetryMirrorPlane(const Value::Object& data);
[[nodiscard]] ri::world::LocalGridSnapVolume BuildLocalGridSnapVolume(const Value::Object& data);
[[nodiscard]] ri::world::HintPartitionVolume BuildHintPartitionVolume(const Value::Object& data);
[[nodiscard]] ri::world::DoorWindowCutoutPrimitive BuildDoorWindowCutoutPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::ProceduralDoorEntity BuildProceduralDoorEntity(const Value::Object& data);
[[nodiscard]] ri::world::CameraConfinementVolume BuildCameraConfinementVolume(const Value::Object& data);
[[nodiscard]] ri::world::LodOverrideVolume BuildLodOverrideVolume(const Value::Object& data);
[[nodiscard]] ri::world::LodSwitchPrimitive BuildLodSwitchPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::SurfaceScatterVolume BuildSurfaceScatterVolume(const Value::Object& data);
[[nodiscard]] ri::world::SplineMeshDeformerPrimitive BuildSplineMeshDeformerPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::SplineDecalRibbonPrimitive BuildSplineDecalRibbonPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::TopologicalUvRemapperVolume BuildTopologicalUvRemapperVolume(const Value::Object& data);
[[nodiscard]] ri::world::TriPlanarNode BuildTriPlanarNode(const Value::Object& data);
[[nodiscard]] ri::world::InstanceCloudPrimitive BuildInstanceCloudPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::VoronoiFracturePrimitive BuildVoronoiFracturePrimitive(const Value::Object& data);
[[nodiscard]] ri::world::MetaballPrimitive BuildMetaballPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::LatticeVolume BuildLatticeVolume(const Value::Object& data);
[[nodiscard]] ri::world::ManifoldSweepPrimitive BuildManifoldSweepPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::TrimSheetSweepPrimitive BuildTrimSheetSweepPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::LSystemBranchPrimitive BuildLSystemBranchPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::GeodesicSpherePrimitive BuildGeodesicSpherePrimitive(const Value::Object& data);
[[nodiscard]] ri::world::ExtrudeAlongNormalPrimitive BuildExtrudeAlongNormalPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::SuperellipsoidPrimitive BuildSuperellipsoidPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::PrimitiveDemoLattice BuildPrimitiveDemoLattice(const Value::Object& data);
[[nodiscard]] ri::world::PrimitiveDemoVoronoi BuildPrimitiveDemoVoronoi(const Value::Object& data);
[[nodiscard]] ri::world::ThickPolygonPrimitive BuildThickPolygonPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::StructuralProfilePrimitive BuildStructuralProfilePrimitive(const Value::Object& data);
[[nodiscard]] ri::world::HalfPipePrimitive BuildHalfPipePrimitive(const Value::Object& data);
[[nodiscard]] ri::world::QuarterPipePrimitive BuildQuarterPipePrimitive(const Value::Object& data);
[[nodiscard]] ri::world::PipeElbowPrimitive BuildPipeElbowPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::TorusSlicePrimitive BuildTorusSlicePrimitive(const Value::Object& data);
[[nodiscard]] ri::world::SplineSweepPrimitive BuildSplineSweepPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::RevolvePrimitive BuildRevolvePrimitive(const Value::Object& data);
[[nodiscard]] ri::world::DomeVaultPrimitive BuildDomeVaultPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::LoftPrimitive BuildLoftPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::NavmeshModifierVolume BuildNavmeshModifierVolume(const Value::Object& data);
[[nodiscard]] ri::world::VisibilityPrimitive BuildVisibilityPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::ReflectionProbeVolume BuildReflectionProbeVolume(const Value::Object& data);
[[nodiscard]] ri::world::LightImportanceVolume BuildLightImportanceVolume(const Value::Object& data);
[[nodiscard]] ri::world::LightPortalVolume BuildLightPortalVolume(const Value::Object& data);
[[nodiscard]] ri::world::VoxelGiBoundsVolume BuildVoxelGiBoundsVolume(const Value::Object& data);
[[nodiscard]] ri::world::LightmapDensityVolume BuildLightmapDensityVolume(const Value::Object& data);
[[nodiscard]] ri::world::ShadowExclusionVolume BuildShadowExclusionVolume(const Value::Object& data);
[[nodiscard]] ri::world::CullingDistanceVolume BuildCullingDistanceVolume(const Value::Object& data);
[[nodiscard]] ri::world::ReferenceImagePlane BuildReferenceImagePlane(const Value::Object& data);
[[nodiscard]] ri::world::Text3dPrimitive BuildText3dPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::AnnotationCommentPrimitive BuildAnnotationCommentPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::MeasureToolPrimitive BuildMeasureToolPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::RenderTargetSurface BuildRenderTargetSurface(const Value::Object& data);
[[nodiscard]] ri::world::PlanarReflectionSurface BuildPlanarReflectionSurface(const Value::Object& data);
[[nodiscard]] ri::world::PassThroughPrimitive BuildPassThroughPrimitive(const Value::Object& data);
[[nodiscard]] ri::world::SkyProjectionSurface BuildSkyProjectionSurface(const Value::Object& data);
[[nodiscard]] ri::world::DynamicInfoPanelSpawner BuildDynamicInfoPanelSpawner(const Value::Object& data);
[[nodiscard]] ri::world::VolumetricEmitterBounds BuildVolumetricEmitterBounds(const Value::Object& data);
[[nodiscard]] ri::world::VolumetricEmitterBounds BuildParticleSpawnVolume(const Value::Object& data);
[[nodiscard]] ri::world::OcclusionPortalVolume BuildOcclusionPortalVolume(const Value::Object& data);
[[nodiscard]] ri::world::PostProcessVolume BuildPostProcessVolume(const Value::Object& data);
[[nodiscard]] ri::world::AudioReverbVolume BuildAudioReverbVolume(const Value::Object& data);
[[nodiscard]] ri::world::AudioOcclusionVolume BuildAudioOcclusionVolume(const Value::Object& data);
[[nodiscard]] ri::world::AmbientAudioVolume BuildAmbientAudioVolume(const Value::Object& data);
[[nodiscard]] ri::world::GenericTriggerVolume BuildGenericTriggerVolume(const Value::Object& data);
[[nodiscard]] ri::world::SpatialQueryVolume BuildSpatialQueryVolume(const Value::Object& data);
[[nodiscard]] ri::world::StreamingLevelVolume BuildStreamingLevelVolume(const Value::Object& data);
[[nodiscard]] ri::world::CheckpointSpawnVolume BuildCheckpointSpawnVolume(const Value::Object& data);
[[nodiscard]] ri::world::TeleportVolume BuildTeleportVolume(const Value::Object& data);
[[nodiscard]] ri::world::LaunchVolume BuildLaunchVolume(const Value::Object& data);
[[nodiscard]] ri::world::AnalyticsHeatmapVolume BuildAnalyticsHeatmapVolume(const Value::Object& data);
[[nodiscard]] ri::world::LocalizedFogVolume BuildLocalizedFogVolume(const Value::Object& data);
[[nodiscard]] ri::world::FogBlockerVolume BuildVolumetricFogBlocker(const Value::Object& data);
[[nodiscard]] ri::world::FluidSimulationVolume BuildFluidSimulationVolume(const Value::Object& data);

} // namespace ri::content
