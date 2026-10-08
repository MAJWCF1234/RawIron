#pragma once

#include "RawIron/Content/NativeSculptDocument.h"
#include "RawIron/Content/BlockCharacterDocument.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/RigAuthoring.h"
#include "RawIron/Scene/Scene.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::scene {

enum class NativeSculptCage {
    Sphere,
    Cube,
    /// Low-poly blocky humanoid (PS1-era proportions) with per-limb boxes.
    PsxHumanoid,
};

enum class NativeSculptBrush {
    Clay,
    Smooth,
    Inflate,
    Flatten,
    Extrude,
};

struct NativeSculptStroke {
    ri::math::Vec3 worldPosition{};
    ri::math::Vec3 worldNormal{0.0f, 1.0f, 0.0f};
    float radius = 0.18f;
    float strength = 0.06f;
    NativeSculptBrush brush = NativeSculptBrush::Clay;
    bool invert = false;
    /// Mirror the stroke across the local YZ (X=0), XZ (Y=0), and/or XY (Z=0) planes.
    bool mirrorX = false;
    bool mirrorY = false;
    bool mirrorZ = false;
};

/// Stroke-granularity undo for native clay. Capture the mesh before a drag begins.
struct NativeSculptUndoStack {
    static constexpr std::size_t kMaxDepth = 32;

    void Capture(const Mesh& mesh);
    void Capture(const ri::content::NativeSculptDocument& document);
    [[nodiscard]] bool CanUndo() const noexcept;
    [[nodiscard]] bool CanRedo() const noexcept;
    bool Undo(Mesh& mesh);
    bool Undo(ri::content::NativeSculptDocument& document);
    bool Redo(Mesh& mesh);
    bool Redo(ri::content::NativeSculptDocument& document);
    void Clear();

private:
    struct Frame {
        Mesh mesh{};
        int segmentsAround = -1;
        int segmentsDown = -1;
        std::vector<std::string> vertexBoneNames{};
        std::vector<std::vector<ri::content::NativeSculptVertexInfluence>> vertexInfluences{};
        bool capturedBind = false;
    };

    std::vector<Frame> undo_{};
    std::vector<Frame> redo_{};
};

[[nodiscard]] NativeSculptCage ParseNativeSculptCage(std::string_view value);
[[nodiscard]] std::string_view NativeSculptCageName(NativeSculptCage cage) noexcept;
[[nodiscard]] std::string_view NativeSculptBrushName(NativeSculptBrush brush) noexcept;

/// Dense cage used as the starting clay or hard-surface block. Primitive type is Custom so displaced
/// vertices stay in the triangle raycast path.
[[nodiscard]] Mesh MakeNativeSculptCageMesh(NativeSculptCage cage,
                                            int segmentsAround = 32,
                                            int segmentsDown = 16,
                                            std::string name = "NativeSculpt");

void RecalculateSculptNormals(Mesh& mesh);
[[nodiscard]] bool ApplyNativeSculptStroke(Mesh& mesh, const NativeSculptStroke& stroke);
/// Region face extrude for hard-surface / box modeling. Adds a rim and cap; one shot per stroke.
/// When `vertexBoneNames` matches the pre-extrude vertex count, appended cap verts inherit the source bone.
/// `vertexInfluences` inherit the same way when they match that count.
[[nodiscard]] bool ApplyNativeSculptFaceExtrude(
    Mesh& mesh,
    const NativeSculptStroke& stroke,
    std::vector<std::string>* vertexBoneNames = nullptr,
    std::vector<std::vector<ri::content::NativeSculptVertexInfluence>>* vertexInfluences = nullptr);
/// 1-to-4 triangle split with welded edge midpoints. Keeps extruded box topology.
[[nodiscard]] bool SubdivideNativeSculptMesh(Mesh& mesh);
/// Rebuild density. Unedited cages project onto a new lattice. Extruded meshes subdivide
/// instead so caps stay raised. Coarsening an extruded cage is refused.
[[nodiscard]] bool RebuildNativeSculptDensity(
    ri::content::NativeSculptDocument& document,
    int segmentsAround,
    int segmentsDown);

struct NativeSculptBindResult {
    bool valid = false;
    std::size_t boundVertexCount = 0;
    std::size_t deformBoneCount = 0;
    std::string summary{};
};

/// Rigid nearest-deform-bone bind. Writes `rigPath`, dominant `vertexBoneNames`, and 1.0 influences.
/// When `replaceAssigned` is false, valid existing names are kept and cleared verts stay unbound.
[[nodiscard]] NativeSculptBindResult BindNativeSculptToRig(
    ri::content::NativeSculptDocument& document,
    const RigDefinition& rig,
    std::string_view rigPath,
    bool replaceAssigned = false);

/// Rest-pose world matrices keyed by bone name, including non-deform control bones.
[[nodiscard]] std::unordered_map<std::string, ri::math::Mat4> RestBoneWorldMatrices(
    const RigDefinition& rig);

/// Skin: `rest * inverse(restBone) * posedBone`, blended across up to four influences when present.
[[nodiscard]] std::size_t SkinRigidMesh(
    Mesh& mesh,
    const std::vector<std::string>& vertexBoneNames,
    const std::vector<ri::math::Vec3>& restPositions,
    const std::vector<ri::math::Vec3>& restNormals,
    const std::unordered_map<std::string, ri::math::Mat4>& restBoneWorld,
    const std::unordered_map<std::string, ri::math::Mat4>& posedBoneWorld,
    const std::vector<std::vector<ri::content::NativeSculptVertexInfluence>>& vertexInfluences = {});

/// Applies skin into `document.mesh` as the new rest pose. Weights/binds are unchanged.
[[nodiscard]] std::size_t BakeNativeSculptSkin(
    ri::content::NativeSculptDocument& document,
    const std::unordered_map<std::string, ri::math::Mat4>& restBoneWorld,
    const std::unordered_map<std::string, ri::math::Mat4>& posedBoneWorld);

[[nodiscard]] Mesh MakeNativeSculptWeightMesh(
    const Mesh& source,
    const std::vector<std::string>& vertexBoneNames,
    std::string_view selectedBone,
    const std::vector<std::vector<ri::content::NativeSculptVertexInfluence>>& vertexInfluences = {});
[[nodiscard]] int InstantiateNativeSculptWeightOverlay(Scene& scene, int parent);
void UpdateNativeSculptWeightOverlay(
    Scene& scene,
    int overlayNode,
    const Mesh& source,
    const std::vector<std::string>& vertexBoneNames,
    std::string_view selectedBone,
    const std::vector<std::vector<ri::content::NativeSculptVertexInfluence>>& vertexInfluences = {});

enum class NativeSculptWeightPaint {
    Assign,
    Add,
    Smooth,
    Clear,
};

/// Vertex-to-bone paint. `Assign` is rigid 1.0, `Add` blends up to four influences, then normalizes.
[[nodiscard]] std::size_t PaintNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    const ri::math::Vec3& worldPosition,
    float radius,
    std::string_view boneName,
    NativeSculptWeightPaint mode,
    bool mirrorX = false,
    bool mirrorY = false,
    bool mirrorZ = false,
    float strength = 1.0f);

[[nodiscard]] std::size_t FloodNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName);

/// Assigns `boneName` only to vertices that currently have no bind.
[[nodiscard]] std::size_t FloodUnboundNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName);

/// Fills unbound verts from bound neighbors (averaged influences). One ring per call.
[[nodiscard]] std::size_t SealUnboundNativeSculptWeightsFromNeighbors(
    ri::content::NativeSculptDocument& document);

/// Moves every influence named `fromBoneName` onto `toBoneName` (merges when both present).
[[nodiscard]] std::size_t TransferNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view fromBoneName,
    std::string_view toBoneName);

/// Exchanges every influence between `leftBoneName` and `rightBoneName`.
[[nodiscard]] std::size_t SwapNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view leftBoneName,
    std::string_view rightBoneName);

/// Inverts `boneName` weight on every vertex (`w' = 1 - w`), then renormalizes.
[[nodiscard]] std::size_t InvertNativeSculptBoneWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName);

/// Scales `boneName` influences by `factor` (> 0), then renormalizes. Returns changed verts.
[[nodiscard]] std::size_t ScaleNativeSculptBoneWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName,
    float factor);

/// Per-vertex weight for `boneName` (0 when unbound). Size matches mesh vertex count.
[[nodiscard]] std::vector<float> ExtractNativeSculptBoneWeights(
    const ri::content::NativeSculptDocument& document,
    std::string_view boneName);

/// Writes `weights[i]` onto `boneName` for each vert (other influences keep relative share).
/// `weights.size()` must equal vertex count. Returns changed verts.
[[nodiscard]] std::size_t ApplyNativeSculptBoneWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName,
    std::span<const float> weights);

/// Expands `boneName` onto adjacent verts that lack it (one ring). Returns newly painted verts.
[[nodiscard]] std::size_t GrowNativeSculptBoneWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName);

/// Shrinks `boneName` off frontier verts that border unpainted neighbors. Returns cleared verts.
[[nodiscard]] std::size_t ShrinkNativeSculptBoneWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName);

/// Snaps verts where `boneName` weight is >= `threshold` to a rigid 1.0 bind on that bone.
[[nodiscard]] std::size_t HardenNativeSculptBoneWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName,
    float threshold = 0.5f);

/// Drops `boneName` influences below `minWeight`, then renormalizes. Returns changed verts.
[[nodiscard]] std::size_t FloorNativeSculptBoneWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName,
    float minWeight = 0.05f);

/// Caps `boneName` influences above `maxWeight`, then renormalizes. Returns changed verts.
[[nodiscard]] std::size_t CeilNativeSculptBoneWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName,
    float maxWeight = 0.85f);

[[nodiscard]] std::size_t RenameNativeSculptBone(
    ri::content::NativeSculptDocument& document,
    std::string_view oldName,
    std::string_view newName);

[[nodiscard]] std::size_t RemoveNativeSculptBone(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName);

/// Clears every vertex bind name and influence list. Returns affected vertex count.
[[nodiscard]] std::size_t ClearNativeSculptWeights(ri::content::NativeSculptDocument& document);

/// Clears `rigPath` and all vertex weights. Returns cleared vertex count.
[[nodiscard]] std::size_t UnbindNativeSculptFromRig(ri::content::NativeSculptDocument& document);

struct NativeSculptWeightAudit {
    std::size_t vertexCount = 0;
    std::size_t boundCount = 0;
    std::size_t unboundCount = 0;
    std::size_t blendedCount = 0;
    std::size_t nonNormalizedCount = 0;
    /// Vertices carrying more than `kMaxInfluences` raw slots (before Cap/Norm).
    std::size_t overInfluencedCount = 0;
};

[[nodiscard]] NativeSculptWeightAudit AuditNativeSculptWeights(
    const ri::content::NativeSculptDocument& document);
/// One laplacian smooth pass over every vertex influence (neighbor average).
[[nodiscard]] std::size_t SmoothNativeSculptWeights(ri::content::NativeSculptDocument& document);
/// Laplacian-smooths only `boneName` weights against neighbors, then renormalizes.
[[nodiscard]] std::size_t SmoothNativeSculptBoneWeights(
    ri::content::NativeSculptDocument& document,
    std::string_view boneName);
[[nodiscard]] std::size_t NormalizeAllNativeSculptWeights(ri::content::NativeSculptDocument& document);
/// Keeps the strongest `maxInfluences` slots per vert (default runtime max), then renormalizes.
[[nodiscard]] std::size_t LimitNativeSculptInfluences(
    ri::content::NativeSculptDocument& document,
    int maxInfluences = ri::content::NativeSculptDocument::kMaxInfluences);
[[nodiscard]] std::size_t PruneAllNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    float minWeight = 0.05f);
/// Drops influences (and dominant names) whose bone is not in `allowedBoneNames`. Renormalizes.
[[nodiscard]] std::size_t StripNativeSculptOrphanInfluences(
    ri::content::NativeSculptDocument& document,
    std::span<const std::string> allowedBoneNames);
/// Copy +X weights to -X partners with left/right bone-name swap. Respects the same mirror planes as paint.
[[nodiscard]] std::size_t MirrorNativeSculptWeights(
    ri::content::NativeSculptDocument& document,
    bool mirrorX = true,
    bool mirrorY = false,
    bool mirrorZ = false);

[[nodiscard]] ri::content::NativeSculptDocument CreateNativeSculptDocument(
    std::string id,
    std::string displayName = {},
    NativeSculptCage cage = NativeSculptCage::Sphere,
    int segmentsAround = 32,
    int segmentsDown = 16);

/// Blocky PSX-style humanoid clay is authored via CreateBlockCharacterSculptDocument + .ri_blockchar.json.

/// World-space low-poly parts for rigid block characters (matches CreateHumanoidRigDefinition rest).
/// `halfExtentTop` zero → no taper. `shape` picks the emitter; `sides`/`bevel` refine it.
enum class PsxPartShape : std::uint8_t {
    Box = 0,
    Prism = 1,
    Dome = 2,
    Wedge = 3,
    Cylinder = 4,
    Spike = 5,
    Slab = 6,
    Bevel = 7,
    Capsule = 8,
};
struct PsxBlockPartDesc {
    std::string boneName{};
    ri::math::Vec3 center{};
    ri::math::Vec3 halfExtent{};
    ri::math::Vec3 halfExtentTop{};
    ri::math::Vec3 rotationDegrees{};
    ri::math::Vec3 baseColor{0.92f, 0.70f, 0.38f};
    PsxPartShape shape = PsxPartShape::Box;
    /// Ring facets for prism/cylinder/dome/capsule (0 = shape default).
    int sides = 0;
    /// Edge chamfer as fraction of min half-extent (bevel/slab), 0..0.45.
    float bevel = 0.0f;
    std::string albedoTexture{};
    float roughness = 0.88f;
    float metallic = 0.0f;
};
/// Convert authored block-character parts into mesh-build descriptors.
[[nodiscard]] std::vector<PsxBlockPartDesc> BlockCharacterPartsToPsxDescs(
    const ri::content::BlockCharacterDocument& document);
/// Local-space mesh for one block part (origin-centered, extents baked). Used by Forge rigid preview.
[[nodiscard]] Mesh MakePsxBlockPartMesh(const PsxBlockPartDesc& part);
/// Build a bindable sculpt mesh from an authored block-character document (no engine-hardcoded scout).
[[nodiscard]] ri::content::NativeSculptDocument CreateBlockCharacterSculptDocument(
    std::string id,
    std::string displayName,
    const ri::content::BlockCharacterDocument& blockCharacter);

[[nodiscard]] int InstantiateNativeSculpt(
    Scene& scene,
    int parent,
    const ri::content::NativeSculptDocument& document);

void WriteNativeSculptMesh(Scene& scene, int sculptNode, const Mesh& mesh);

[[nodiscard]] Mesh MakeNativeSculptWireframeMesh(const Mesh& source, float thickness = 0.012f);
[[nodiscard]] int InstantiateNativeSculptWireframe(Scene& scene, int parent, const Mesh& source);
void UpdateNativeSculptWireframe(Scene& scene, int wireframeNode, const Mesh& source);

[[nodiscard]] Mesh MakeNativeSculptNormalsMesh(const Mesh& source, float length = 0.07f);
[[nodiscard]] int InstantiateNativeSculptNormals(Scene& scene, int parent, const Mesh& source);
void UpdateNativeSculptNormals(Scene& scene, int normalsNode, const Mesh& source);

[[nodiscard]] Mesh MakeNativeSculptCollisionBoundsMesh(const Mesh& source, float thickness = 0.016f);
[[nodiscard]] int InstantiateNativeSculptCollisionBounds(Scene& scene, int parent, const Mesh& source);
void UpdateNativeSculptCollisionBounds(Scene& scene, int collisionNode, const Mesh& source);

[[nodiscard]] int InstantiateNativeSculptBrushCursor(Scene& scene, int parent);
void UpdateNativeSculptBrushCursor(
    Scene& scene,
    int cursorNode,
    const ri::math::Vec3& worldPosition,
    float radius,
    bool visible);

} // namespace ri::scene
