#pragma once

#include "RawIron/Math/Vec2.h"
#include "RawIron/Math/Vec3.h"

#include <string>
#include <vector>

namespace ri::scene {

enum class PrimitiveType {
    Custom,
    Cube,
    Plane,
    Sphere,
};

enum class ShadingModel {
    Unlit,
    Lit,
};

enum class MaterialStyle {
    Standard,
    Retro,
    Layered,
    MixedMedia,
    Crystal,
};

enum class MaterialWorkflow {
    MetalRough,
    SpecGloss,
};

enum class StructuralBrushOperation {
    Unspecified,
    Solid,
    Subtract,
    Intersect,
    Stamp,
    Merge,
    Detail,
};

enum class StructuralBrushSemanticRole {
    Structure,
    Wall,
    Floor,
    Ceiling,
    Pillar,
    Stair,
    Portal,
    Trim,
    Cover,
    Water,
    Trigger,
    Decor,
    Volume,
};

enum class StructuralBrushCollisionPolicy {
    Solid,
    None,
    Query,
    Player,
    Detail,
    Custom,
};

enum class StructuralBrushVisibilityPolicy {
    Ignored,
    Occluder,
    Portal,
    AntiPortal,
    Transparent,
};

enum class StructuralBrushNavigationPolicy {
    Ignored,
    Walkable,
    Blocker,
    Jump,
    Cover,
    Ladder,
};

enum class StructuralBrushRebuildScope {
    Local,
    Region,
    Global,
    Manual,
};

struct StructuralBrushMetadata {
    std::string brushId{};
    std::string region{};
    StructuralBrushOperation operation = StructuralBrushOperation::Unspecified;
    StructuralBrushSemanticRole role = StructuralBrushSemanticRole::Structure;
    StructuralBrushCollisionPolicy collision = StructuralBrushCollisionPolicy::Solid;
    StructuralBrushVisibilityPolicy visibility = StructuralBrushVisibilityPolicy::Ignored;
    StructuralBrushNavigationPolicy navigation = StructuralBrushNavigationPolicy::Ignored;
    StructuralBrushRebuildScope rebuildScope = StructuralBrushRebuildScope::Local;
};

enum class ProjectionType {
    Perspective,
    Orthographic,
};

enum class LightType {
    Directional,
    Point,
    Spot,
};

struct Material {
    std::string name;
    ShadingModel shadingModel = ShadingModel::Lit;
    MaterialStyle materialStyle = MaterialStyle::Standard;
    MaterialWorkflow materialWorkflow = MaterialWorkflow::MetalRough;
    ri::math::Vec3 baseColor{1.0f, 1.0f, 1.0f};
    /// Filename under the software preview texture root (default: in-repo `Assets/Textures`).
    std::string baseColorTexture{};
    /// Optional explicit animation frames under the same texture root.
    std::vector<std::string> baseColorTextureFrames{};
    float baseColorTextureFramesPerSecond = 0.0f;
    /// Optional flipbook atlas encoded inside `baseColorTexture`.
    int baseColorTextureAtlasColumns = 1;
    int baseColorTextureAtlasRows = 1;
    int baseColorTextureAtlasFrameCount = 0;
    float baseColorTextureAtlasFramesPerSecond = 0.0f;
    ri::math::Vec2 textureTiling{1.0f, 1.0f};
    ri::math::Vec3 emissiveColor{0.0f, 0.0f, 0.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    float opacity = 1.0f;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool transparent = false;
    /// Software preview (and submission sorting): additive tint dest += fragment * opacity; does not write depth.
    bool additiveBlend = false;
    /// Optional tangent-space normal map under the texture root.
    std::string normalTexture{};
    /// Optional packed occlusion/roughness/metallic map (ORM in glTF convention).
    std::string ormTexture{};
    /// Optional standalone roughness map when no packed ORM map is present.
    std::string roughnessTexture{};
    /// Optional standalone metallic map when no packed ORM map is present.
    std::string metallicTexture{};
    /// Optional emissive color map.
    std::string emissiveTexture{};
    /// Optional opacity/alpha texture for cutout/transparent materials.
    std::string opacityTexture{};
    /// Optional standalone occlusion (ambient-occlusion) map.
    std::string occlusionTexture{};
    /// Optional extra texture used by layered materials for macro/detail breakup.
    std::string detailTexture{};
    /// UVs were baked into mesh texcoords in world tile units; `textureTiling` is {1,1}.
    bool bakedWorldTileUv = false;
    /// Unity BOTD-style albedo maps pack smoothness in alpha (not opacity).
    bool albedoAlphaIsSmoothness = false;
};

struct Mesh {
    std::string name;
    PrimitiveType primitive = PrimitiveType::Custom;
    int vertexCount = 0;
    int indexCount = 0;
    std::vector<ri::math::Vec3> positions;
    /// Optional vertex normals parallel to `positions`; preferred by native/software preview when present.
    std::vector<ri::math::Vec3> normals;
    /// When non-empty, must match `positions.size()` for textured custom meshes in the software preview.
    std::vector<ri::math::Vec2> texCoords;
    std::vector<int> indices;
};

struct Camera {
    std::string name;
    ProjectionType projection = ProjectionType::Perspective;
    float fieldOfViewDegrees = 60.0f;
    float nearClip = 0.1f;
    float farClip = 1000.0f;
};

struct Light {
    std::string name;
    LightType type = LightType::Directional;
    ri::math::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    float spotAngleDegrees = 45.0f;
};

/// Primary runtime discriminator for [`CameraConfinementVolume`] script and camera-controller integration.
enum class CameraConfinementBehavior {
    RegionClamp,
    FramingCorridor,
    HoldAttention,
    PathGuided,
    LimitFreeLook,
    AnchorMotion,
    CutsceneSync,
    MediaPresentation,
};

/// Optional authoring / tooling tag for cinematic beats (overlap resolution still uses numeric `priority`).
enum class CameraConfinementPurpose {
    Unspecified,
    CornerReveal,
    ForcedObservation,
    GuidedPath,
    ProjectedMedia,
    CutsceneStaging,
};

/// Axis-aligned box in **node local space**: half-extents along local X/Y/Z from the node's origin.
/// World orientation and position come from the node's transform; not a gameplay mesh—an authored control volume.
struct CameraConfinementVolume {
    std::string name;
    /// Short label for editor overlays (for example `"Corner Reveal A"`).
    std::string editorLabel{};
    ri::math::Vec3 halfExtents{1.0f, 1.0f, 1.0f};
    CameraConfinementBehavior behavior = CameraConfinementBehavior::RegionClamp;
    CameraConfinementPurpose stagingPurpose = CameraConfinementPurpose::Unspecified;
    /// `-1` means unset (same sentinel as `Scene::kInvalidHandle`).
    int focusTargetNode = -1;
    /// Larger wins when multiple active volumes overlap at a sample point.
    int priority = 0;
    bool active = true;
};

std::string ToString(PrimitiveType primitive);
std::string ToString(ShadingModel shadingModel);
std::string ToString(MaterialStyle materialStyle);
std::string ToString(MaterialWorkflow materialWorkflow);
std::string ToString(StructuralBrushOperation operation);
std::string ToString(StructuralBrushSemanticRole role);
std::string ToString(StructuralBrushCollisionPolicy policy);
std::string ToString(StructuralBrushVisibilityPolicy policy);
std::string ToString(StructuralBrushNavigationPolicy policy);
std::string ToString(StructuralBrushRebuildScope scope);
std::string ToString(ProjectionType projectionType);
std::string ToString(LightType lightType);
std::string ToString(CameraConfinementBehavior behavior);
std::string ToString(CameraConfinementPurpose purpose);

} // namespace ri::scene
