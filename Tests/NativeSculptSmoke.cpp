#include "RawIron/Scene/NativeSculpt.h"
#include "RawIron/Scene/Raycast.h"
#include "RawIron/Scene/RigAuthoring.h"
#include "RawIron/Content/BlockCharacterDocument.h"
#include "RawIron/Math/Mat4.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
    ri::content::NativeSculptDocument sphere =
        ri::scene::CreateNativeSculptDocument("clay ball", "Clay Ball", ri::scene::NativeSculptCage::Sphere, 24, 12);
    const auto sphereReport = ri::content::ValidateNativeSculptDocument(sphere);
    if (!sphereReport.valid || sphere.mesh.positions.size() < 100U || sphere.mesh.indices.size() < 300U
        || sphere.mesh.primitive != ri::scene::PrimitiveType::Custom) {
        std::cerr << "Sphere cage was not a dense custom sculpt mesh.\n";
        return EXIT_FAILURE;
    }

    const ri::math::Vec3 before = sphere.mesh.positions.front();
    const bool stroked = ri::scene::ApplyNativeSculptStroke(sphere.mesh, ri::scene::NativeSculptStroke{
        .worldPosition = before,
        .worldNormal = {0.0f, 1.0f, 0.0f},
        .radius = 0.35f,
        .strength = 0.12f,
        .brush = ri::scene::NativeSculptBrush::Clay,
    });
    if (!stroked || ri::math::Distance(before, sphere.mesh.positions.front()) < 0.001f) {
        std::cerr << "Clay stroke did not displace vertices.\n";
        return EXIT_FAILURE;
    }

    ri::scene::Scene scene{"Sculpt Preview"};
    const int node = ri::scene::InstantiateNativeSculpt(scene, ri::scene::kInvalidHandle, sphere);
    if (node == ri::scene::kInvalidHandle) {
        std::cerr << "Could not instantiate sculpt mesh.\n";
        return EXIT_FAILURE;
    }
    const ri::scene::Ray ray{
        .origin = {0.0f, 1.5f, 0.0f},
        .direction = {0.0f, -1.0f, 0.0f},
    };
    const auto hit = ri::scene::RaycastNode(scene, node, ray);
    if (!hit.has_value() || hit->node != node) {
        std::cerr << "Sculpt mesh was not raycastable.\n";
        return EXIT_FAILURE;
    }

    const ri::math::Vec3 inflatedBefore = sphere.mesh.positions.front();
    if (!ri::scene::ApplyNativeSculptStroke(sphere.mesh, ri::scene::NativeSculptStroke{
            .worldPosition = inflatedBefore,
            .worldNormal = {0.0f, 1.0f, 0.0f},
            .radius = 0.4f,
            .strength = 0.08f,
            .brush = ri::scene::NativeSculptBrush::Inflate,
        })
        || ri::math::Distance(inflatedBefore, sphere.mesh.positions.front()) < 0.0005f) {
        std::cerr << "Inflate brush did not displace vertices.\n";
        return EXIT_FAILURE;
    }

    ri::scene::NativeSculptUndoStack undo{};
    const ri::scene::Mesh beforeMirror = sphere.mesh;
    undo.Capture(beforeMirror);
    const ri::math::Vec3 plusX{0.45f, 0.0f, 0.0f};
    const ri::math::Vec3 minusX{-0.45f, 0.0f, 0.0f};
    float plusBefore = 0.0f;
    float minusBefore = 0.0f;
    for (const ri::math::Vec3& position : sphere.mesh.positions) {
        if (position.x > plusBefore) {
            plusBefore = position.x;
        }
        if (position.x < minusBefore) {
            minusBefore = position.x;
        }
    }
    if (!ri::scene::ApplyNativeSculptStroke(sphere.mesh, ri::scene::NativeSculptStroke{
            .worldPosition = plusX,
            .worldNormal = {1.0f, 0.0f, 0.0f},
            .radius = 0.35f,
            .strength = 0.2f,
            .brush = ri::scene::NativeSculptBrush::Clay,
            .mirrorX = true,
        })) {
        std::cerr << "Mirrored clay stroke failed.\n";
        return EXIT_FAILURE;
    }
    float plusAfter = 0.0f;
    float minusAfter = 0.0f;
    for (const ri::math::Vec3& position : sphere.mesh.positions) {
        if (position.x > plusAfter) {
            plusAfter = position.x;
        }
        if (position.x < minusAfter) {
            minusAfter = position.x;
        }
    }
    if (plusAfter <= plusBefore + 0.01f || minusAfter >= minusBefore - 0.01f) {
        std::cerr << "X-mirror did not push both sides.\n";
        return EXIT_FAILURE;
    }

    undo.Capture(sphere.mesh);
    float plusYBefore = 0.0f;
    float minusYBefore = 0.0f;
    float plusZBefore = 0.0f;
    float minusZBefore = 0.0f;
    for (const ri::math::Vec3& position : sphere.mesh.positions) {
        plusYBefore = std::max(plusYBefore, position.y);
        minusYBefore = std::min(minusYBefore, position.y);
        plusZBefore = std::max(plusZBefore, position.z);
        minusZBefore = std::min(minusZBefore, position.z);
    }
    if (!ri::scene::ApplyNativeSculptStroke(sphere.mesh, ri::scene::NativeSculptStroke{
            .worldPosition = {0.0f, 0.45f, 0.0f},
            .worldNormal = {0.0f, 1.0f, 0.0f},
            .radius = 0.35f,
            .strength = 0.2f,
            .brush = ri::scene::NativeSculptBrush::Clay,
            .mirrorY = true,
        })) {
        std::cerr << "Y-mirrored clay stroke failed.\n";
        return EXIT_FAILURE;
    }
    float plusYAfter = 0.0f;
    float minusYAfter = 0.0f;
    for (const ri::math::Vec3& position : sphere.mesh.positions) {
        plusYAfter = std::max(plusYAfter, position.y);
        minusYAfter = std::min(minusYAfter, position.y);
    }
    if (plusYAfter <= plusYBefore + 0.01f || minusYAfter >= minusYBefore - 0.01f) {
        std::cerr << "Y-mirror did not push both sides.\n";
        return EXIT_FAILURE;
    }
    if (!undo.Undo(sphere.mesh)) {
        std::cerr << "Could not restore mesh after Y-mirror.\n";
        return EXIT_FAILURE;
    }
    undo.Capture(sphere.mesh);
    if (!ri::scene::ApplyNativeSculptStroke(sphere.mesh, ri::scene::NativeSculptStroke{
            .worldPosition = {0.0f, 0.0f, 0.45f},
            .worldNormal = {0.0f, 0.0f, 1.0f},
            .radius = 0.35f,
            .strength = 0.2f,
            .brush = ri::scene::NativeSculptBrush::Clay,
            .mirrorZ = true,
        })) {
        std::cerr << "Z-mirrored clay stroke failed.\n";
        return EXIT_FAILURE;
    }
    float plusZAfter = 0.0f;
    float minusZAfter = 0.0f;
    for (const ri::math::Vec3& position : sphere.mesh.positions) {
        plusZAfter = std::max(plusZAfter, position.z);
        minusZAfter = std::min(minusZAfter, position.z);
    }
    if (plusZAfter <= plusZBefore + 0.01f || minusZAfter >= minusZBefore - 0.01f) {
        std::cerr << "Z-mirror did not push both sides.\n";
        return EXIT_FAILURE;
    }
    if (!undo.Undo(sphere.mesh)) {
        std::cerr << "Could not restore mesh after Z-mirror.\n";
        return EXIT_FAILURE;
    }
    if (!undo.Undo(sphere.mesh) || sphere.mesh.positions.size() != beforeMirror.positions.size()
        || ri::math::Distance(sphere.mesh.positions.front(), beforeMirror.positions.front()) > 0.0001f
        || !undo.CanRedo() || !undo.Redo(sphere.mesh)) {
        std::cerr << "Sculpt undo/redo failed.\n";
        return EXIT_FAILURE;
    }

    const ri::scene::Mesh wire = ri::scene::MakeNativeSculptWireframeMesh(sphere.mesh);
    if (wire.indices.size() < 36U || wire.positions.size() < 8U) {
        std::cerr << "Wireframe overlay mesh was empty.\n";
        return EXIT_FAILURE;
    }
    const ri::scene::Mesh normals = ri::scene::MakeNativeSculptNormalsMesh(sphere.mesh);
    if (normals.positions.size() < 8U || normals.indices.size() < 36U) {
        std::cerr << "Normals overlay mesh was empty.\n";
        return EXIT_FAILURE;
    }
    const ri::scene::Mesh collision = ri::scene::MakeNativeSculptCollisionBoundsMesh(sphere.mesh);
    if (collision.positions.size() != 96U || collision.indices.size() != 432U) {
        std::cerr << "Collision overlay did not build a 12-edge AABB.\n";
        return EXIT_FAILURE;
    }
    const int cursor = ri::scene::InstantiateNativeSculptBrushCursor(scene, ri::scene::kInvalidHandle);
    const int wireNode = ri::scene::InstantiateNativeSculptWireframe(scene, ri::scene::kInvalidHandle, sphere.mesh);
    const int normalsNode = ri::scene::InstantiateNativeSculptNormals(scene, ri::scene::kInvalidHandle, sphere.mesh);
    const int collisionNode =
        ri::scene::InstantiateNativeSculptCollisionBounds(scene, ri::scene::kInvalidHandle, sphere.mesh);
    if (cursor == ri::scene::kInvalidHandle || wireNode == ri::scene::kInvalidHandle
        || normalsNode == ri::scene::kInvalidHandle || collisionNode == ri::scene::kInvalidHandle) {
        std::cerr << "Sculpt overlay helpers did not instantiate.\n";
        return EXIT_FAILURE;
    }
    ri::scene::UpdateNativeSculptBrushCursor(scene, cursor, hit->position, 0.2f, true);
    if (scene.GetNode(cursor).localTransform.scale.x < 0.3f) {
        std::cerr << "Brush cursor scale was not driven by radius.\n";
        return EXIT_FAILURE;
    }
    ri::scene::WriteNativeSculptMesh(scene, node, sphere.mesh);

    const std::string json = ri::content::SerializeNativeSculptDocument(sphere);
    const auto parsed = ri::content::ParseNativeSculptDocument(json);
    if (!parsed.has_value() || parsed->mesh.positions.size() != sphere.mesh.positions.size()) {
        std::cerr << "Sculpted mesh did not serialize.\n";
        return EXIT_FAILURE;
    }

    ri::content::NativeSculptDocument cube =
        ri::scene::CreateNativeSculptDocument("block", "Block", ri::scene::NativeSculptCage::Cube, 8, 8);
    if (!ri::content::ValidateNativeSculptDocument(cube).valid || cube.cage != "cube") {
        std::cerr << "Cube cage failed validation.\n";
        return EXIT_FAILURE;
    }
    const std::size_t cubeVerts = cube.mesh.positions.size();
    float cubeTop = 0.0f;
    for (const ri::math::Vec3& position : cube.mesh.positions) {
        cubeTop = std::max(cubeTop, position.y);
    }
    if (!ri::scene::ApplyNativeSculptFaceExtrude(cube.mesh, ri::scene::NativeSculptStroke{
            .worldPosition = {0.0f, 0.5f, 0.0f},
            .worldNormal = {0.0f, 1.0f, 0.0f},
            .radius = 0.35f,
            .strength = 0.12f,
            .brush = ri::scene::NativeSculptBrush::Extrude,
        })
        || cube.mesh.positions.size() <= cubeVerts) {
        std::cerr << "Cube face extrude did not add vertices.\n";
        return EXIT_FAILURE;
    }
    float cubeTopAfter = 0.0f;
    for (const ri::math::Vec3& position : cube.mesh.positions) {
        cubeTopAfter = std::max(cubeTopAfter, position.y);
    }
    if (cubeTopAfter <= cubeTop + 0.02f) {
        std::cerr << "Cube face extrude did not raise the cap.\n";
        return EXIT_FAILURE;
    }
    const std::size_t extrudedVerts = cube.mesh.positions.size();
    if (!ri::scene::RebuildNativeSculptDensity(cube, 16, 16)
        || cube.mesh.positions.size() <= extrudedVerts
        || cube.segmentsAround <= 8) {
        std::cerr << "Extruded cube density did not subdivide in place.\n";
        return EXIT_FAILURE;
    }
    float cubeTopAfterRemesh = 0.0f;
    for (const ri::math::Vec3& position : cube.mesh.positions) {
        cubeTopAfterRemesh = std::max(cubeTopAfterRemesh, position.y);
    }
    if (cubeTopAfterRemesh <= cubeTop + 0.02f) {
        std::cerr << "Density remesh flattened the extruded cube cap.\n";
        return EXIT_FAILURE;
    }
    if (ri::scene::RebuildNativeSculptDensity(cube, 8, 8)) {
        std::cerr << "Extruded cube should refuse coarsening.\n";
        return EXIT_FAILURE;
    }

    const std::size_t beforeDensity = sphere.mesh.positions.size();
    const int beforeAround = sphere.segmentsAround;
    if (!ri::scene::RebuildNativeSculptDensity(sphere, 40, 20)
        || sphere.mesh.positions.size() <= beforeDensity
        || sphere.segmentsAround <= beforeAround) {
        std::cerr << "Density remesh did not increase cage resolution.\n";
        return EXIT_FAILURE;
    }
    const ri::math::Vec3 flattenAt = sphere.mesh.positions.front();
    const float flattenBefore = flattenAt.y;
    if (!ri::scene::ApplyNativeSculptStroke(sphere.mesh, ri::scene::NativeSculptStroke{
            .worldPosition = {flattenAt.x, 0.0f, flattenAt.z},
            .worldNormal = {0.0f, 1.0f, 0.0f},
            .radius = 0.8f,
            .strength = 0.2f,
            .brush = ri::scene::NativeSculptBrush::Flatten,
        })
        || std::abs(sphere.mesh.positions.front().y) >= std::abs(flattenBefore)) {
        std::cerr << "Flatten brush did not pull vertices toward the plane.\n";
        return EXIT_FAILURE;
    }

    const ri::scene::RigDefinition humanoid =
        ri::scene::CreateHumanoidRigDefinition("bind_fixture", "Bind Fixture");
    const ri::scene::NativeSculptBindResult bind =
        ri::scene::BindNativeSculptToRig(sphere, humanoid, "rigs/bind_fixture.ri_rig.json");
    if (!bind.valid
        || bind.boundVertexCount != sphere.mesh.positions.size()
        || sphere.vertexBoneNames.size() != sphere.mesh.positions.size()
        || sphere.rigPath != "rigs/bind_fixture.ri_rig.json"
        || bind.deformBoneCount == 0U) {
        std::cerr << "Nearest-bone sculpt bind failed.\n";
        return EXIT_FAILURE;
    }
    for (const std::string& boneName : sphere.vertexBoneNames) {
        if (boneName.empty()) {
            std::cerr << "Sculpt bind left an unnamed vertex.\n";
            return EXIT_FAILURE;
        }
    }
    const std::string nearestFront = sphere.vertexBoneNames.front();
    sphere.vertexBoneNames.front() = nearestFront == "left_hand" ? "right_hand" : "left_hand";
    const std::string paintedFront = sphere.vertexBoneNames.front();
    const ri::scene::NativeSculptBindResult kept = ri::scene::BindNativeSculptToRig(
        sphere, humanoid, "rigs/bind_fixture.ri_rig.json");
    if (!kept.valid || sphere.vertexBoneNames.front() != paintedFront) {
        std::cerr << "Rebind wiped painted vertex weights.\n";
        return EXIT_FAILURE;
    }
    const ri::scene::NativeSculptBindResult replaced = ri::scene::BindNativeSculptToRig(
        sphere, humanoid, "rigs/bind_fixture.ri_rig.json", true);
    if (!replaced.valid || sphere.vertexBoneNames.front() != nearestFront) {
        std::cerr << "Replace bind did not restore nearest-bone weights.\n";
        return EXIT_FAILURE;
    }
    ri::content::NativeSculptDocument emptyMesh = sphere;
    emptyMesh.mesh.positions.clear();
    emptyMesh.mesh.indices.clear();
    emptyMesh.mesh.vertexCount = 0;
    emptyMesh.mesh.indexCount = 0;
    if (ri::scene::BindNativeSculptToRig(emptyMesh, humanoid, "rigs/bind_fixture.ri_rig.json").valid) {
        std::cerr << "Empty sculpt bind was accepted.\n";
        return EXIT_FAILURE;
    }

    ri::scene::Mesh skinned = sphere.mesh;
    const auto restWorld = ri::scene::RestBoneWorldMatrices(humanoid);
    auto posedWorld = restWorld;
    const std::string& driven = sphere.vertexBoneNames.front();
    posedWorld[driven] = ri::math::Multiply(
        ri::math::TranslationMatrix({0.0f, 0.25f, 0.0f}), restWorld.at(driven));
    const std::size_t moved = ri::scene::SkinRigidMesh(
        skinned,
        sphere.vertexBoneNames,
        sphere.mesh.positions,
        sphere.mesh.normals,
        restWorld,
        posedWorld);
    if (moved == 0U
        || std::abs(skinned.positions.front().y - sphere.mesh.positions.front().y) < 0.05f) {
        std::cerr << "Rigid sculpt skin did not follow the posed bone.\n";
        return EXIT_FAILURE;
    }
    const float rigidDelta = skinned.positions.front().y - sphere.mesh.positions.front().y;
    const std::size_t identitySkinned = ri::scene::SkinRigidMesh(
        skinned,
        sphere.vertexBoneNames,
        sphere.mesh.positions,
        sphere.mesh.normals,
        restWorld,
        restWorld);
    if (identitySkinned == 0U
        || ri::math::Distance(skinned.positions.front(), sphere.mesh.positions.front()) > 0.0005f) {
        std::cerr << "Identity sculpt skin moved rest vertices.\n";
        return EXIT_FAILURE;
    }
    {
        ri::content::NativeSculptDocument baked = sphere;
        const float beforeY = baked.mesh.positions.front().y;
        if (ri::scene::BakeNativeSculptSkin(baked, restWorld, posedWorld) == 0U
            || std::abs(baked.mesh.positions.front().y - beforeY) < 0.05f) {
            std::cerr << "Bake sculpt skin did not rewrite rest positions.\n";
            return EXIT_FAILURE;
        }
        const float bakedY = baked.mesh.positions.front().y;
        if (ri::scene::BakeNativeSculptSkin(baked, restWorld, restWorld) == 0U
            || std::abs(baked.mesh.positions.front().y - bakedY) > 0.0005f) {
            std::cerr << "Identity bake sculpt skin moved already-baked rest.\n";
            return EXIT_FAILURE;
        }
    }
    std::string otherBone = "hips";
    for (const ri::scene::RigBone& bone : humanoid.bones) {
        if (bone.deform && bone.name != driven) {
            otherBone = bone.name;
            break;
        }
    }
    ri::content::NativeSculptDocument blended = sphere;
    blended.vertexInfluences.assign(blended.mesh.positions.size(), {});
    blended.vertexInfluences.front() = {
        {.boneName = driven, .weight = 0.5f},
        {.boneName = otherBone, .weight = 0.5f},
    };
    ri::scene::Mesh halfSkinned = blended.mesh;
    const std::size_t halfMoved = ri::scene::SkinRigidMesh(
        halfSkinned,
        blended.vertexBoneNames,
        blended.mesh.positions,
        blended.mesh.normals,
        restWorld,
        posedWorld,
        blended.vertexInfluences);
    const float blendDelta = halfSkinned.positions.front().y - blended.mesh.positions.front().y;
    if (halfMoved == 0U || rigidDelta <= 0.05f
        || std::abs(blendDelta - rigidDelta * 0.5f) > 0.02f) {
        std::cerr << "Blended sculpt skin did not split the posed bone equally.\n";
        return EXIT_FAILURE;
    }
    const ri::scene::Mesh weights = ri::scene::MakeNativeSculptWeightMesh(
        sphere.mesh, sphere.vertexBoneNames, driven);
    if (weights.indices.size() < 3U) {
        std::cerr << "Weight overlay produced no triangles for a bound bone.\n";
        return EXIT_FAILURE;
    }
    const ri::scene::Mesh blendedWeights = ri::scene::MakeNativeSculptWeightMesh(
        blended.mesh, blended.vertexBoneNames, driven, blended.vertexInfluences);
    if (blendedWeights.indices.size() < 3U) {
        std::cerr << "Weight overlay hid a 0.5 blended influence.\n";
        return EXIT_FAILURE;
    }

    ri::content::NativeSculptDocument paint =
        ri::scene::CreateNativeSculptDocument("paint", "Paint", ri::scene::NativeSculptCage::Sphere, 16, 8);
    const ri::math::Vec3 paintTip = paint.mesh.positions.front();
    if (ri::scene::PaintNativeSculptWeights(
            paint, paintTip, 0.35f, "", ri::scene::NativeSculptWeightPaint::Assign)
        != 0U) {
        std::cerr << "Assign paint without a bone name changed vertices.\n";
        return EXIT_FAILURE;
    }
    const std::size_t assigned = ri::scene::PaintNativeSculptWeights(
        paint, paintTip, 0.35f, "hand_l", ri::scene::NativeSculptWeightPaint::Assign);
    if (assigned == 0U
        || paint.vertexBoneNames.size() != paint.mesh.positions.size()
        || paint.vertexBoneNames.front() != "hand_l") {
        std::cerr << "Assign paint did not bind vertices near the stroke.\n";
        return EXIT_FAILURE;
    }
    std::size_t plusBound = 0;
    std::size_t minusBound = 0;
    const std::size_t mirrored = ri::scene::PaintNativeSculptWeights(
        paint,
        {0.45f, 0.0f, 0.0f},
        0.28f,
        "hand_r",
        ri::scene::NativeSculptWeightPaint::Assign,
        true);
    if (mirrored == 0U) {
        std::cerr << "Mirrored weight paint changed no vertices.\n";
        return EXIT_FAILURE;
    }
    for (std::size_t vertex = 0; vertex < paint.mesh.positions.size(); ++vertex) {
        if (paint.vertexBoneNames[vertex] != "hand_r") {
            continue;
        }
        if (paint.mesh.positions[vertex].x > 0.05f) {
            ++plusBound;
        } else if (paint.mesh.positions[vertex].x < -0.05f) {
            ++minusBound;
        }
    }
    if (plusBound == 0U || minusBound == 0U) {
        std::cerr << "X-mirror weight paint did not assign both sides.\n";
        return EXIT_FAILURE;
    }

    ri::scene::NativeSculptUndoStack paintUndo{};
    paintUndo.Capture(paint);
    const std::size_t cleared = ri::scene::PaintNativeSculptWeights(
        paint, paintTip, 0.35f, "hand_l", ri::scene::NativeSculptWeightPaint::Clear);
    if (cleared == 0U || paint.vertexBoneNames.front() == "hand_l") {
        std::cerr << "Clear paint did not unbind vertices.\n";
        return EXIT_FAILURE;
    }
    if (!paintUndo.Undo(paint) || paint.vertexBoneNames.front() != "hand_l"
        || paint.vertexInfluences.size() != paint.mesh.positions.size()
        || paint.vertexInfluences.front().size() != 1U
        || paint.vertexInfluences.front().front().boneName != "hand_l") {
        std::cerr << "Undo did not restore painted bone names.\n";
        return EXIT_FAILURE;
    }
    if (!paintUndo.Redo(paint) || paint.vertexBoneNames.front() == "hand_l") {
        std::cerr << "Redo did not restore cleared bone names.\n";
        return EXIT_FAILURE;
    }

    if (ri::scene::FloodNativeSculptWeights(paint, "") != 0U) {
        std::cerr << "Flood without a bone name changed vertices.\n";
        return EXIT_FAILURE;
    }
    const std::size_t flooded = ri::scene::FloodNativeSculptWeights(paint, "hips");
    if (flooded == 0U) {
        std::cerr << "Flood paint changed no vertices.\n";
        return EXIT_FAILURE;
    }
    for (const std::string& boneName : paint.vertexBoneNames) {
        if (boneName != "hips") {
            std::cerr << "Flood paint left a vertex unbound from hips.\n";
            return EXIT_FAILURE;
        }
    }
    // Leave one free vertex, flood only unbound onto spine, keep hips elsewhere.
    if (!paint.vertexBoneNames.empty()) {
        paint.vertexBoneNames.front().clear();
        if (!paint.vertexInfluences.empty()) {
            paint.vertexInfluences.front().clear();
        }
    }
    if (ri::scene::FloodUnboundNativeSculptWeights(paint, "spine") != 1U
        || paint.vertexBoneNames.front() != "spine") {
        std::cerr << "Flood unbound missed the free vertex.\n";
        return EXIT_FAILURE;
    }
    if (std::any_of(
            paint.vertexBoneNames.begin() + 1,
            paint.vertexBoneNames.end(),
            [](const std::string& boneName) { return boneName != "hips"; })) {
        std::cerr << "Flood unbound overwrote already-bound verts.\n";
        return EXIT_FAILURE;
    }
        if (ri::scene::TransferNativeSculptWeights(paint, "spine", "hips") != 1U
            || paint.vertexBoneNames.front() != "hips") {
            std::cerr << "Transfer weights did not move the free vertex onto hips.\n";
            return EXIT_FAILURE;
        }
        // Split hips/spine across verts, then swap partner names.
        paint.vertexBoneNames.front() = "hand_l";
        if (!paint.vertexInfluences.empty()) {
            paint.vertexInfluences.front() = {{"hand_l", 1.0f}};
        }
        if (paint.vertexBoneNames.size() > 1U) {
            paint.vertexBoneNames[1] = "hand_r";
            if (paint.vertexInfluences.size() > 1U) {
                paint.vertexInfluences[1] = {{"hand_r", 1.0f}};
            }
        }
        if (ri::scene::SwapNativeSculptWeights(paint, "hand_l", "hand_r") < 2U
            || paint.vertexBoneNames.front() != "hand_r"
            || (paint.vertexBoneNames.size() > 1U && paint.vertexBoneNames[1] != "hand_l")) {
            std::cerr << "Swap weights did not exchange left/right binds.\n";
            return EXIT_FAILURE;
        }
        if (ri::scene::InvertNativeSculptBoneWeights(paint, "hand_r") == 0U
            || paint.vertexBoneNames.front() == "hand_r") {
            std::cerr << "Invert weights did not clear the rigid hand_r bind.\n";
            return EXIT_FAILURE;
        }
        paint.vertexBoneNames.front() = "hand_l";
        if (!paint.vertexInfluences.empty()) {
            paint.vertexInfluences.front() = {
                ri::content::NativeSculptVertexInfluence{.boneName = "hand_l", .weight = 0.5f},
                ri::content::NativeSculptVertexInfluence{.boneName = "hips", .weight = 0.5f},
            };
        }
        if (ri::scene::ScaleNativeSculptBoneWeights(paint, "hand_l", 2.0f) == 0U
            || paint.vertexInfluences.front().empty()
            || paint.vertexInfluences.front().front().boneName != "hand_l"
            || paint.vertexInfluences.front().front().weight < 0.65f) {
            std::cerr << "Scale bone weights did not strengthen hand_l.\n";
            return EXIT_FAILURE;
        }
        // Rigid ½W must leave unbound remainder; ×2W on rigid 1.0 is a no-op.
        {
            paint.vertexBoneNames.front() = "hand_l";
            paint.vertexInfluences.front() = {
                ri::content::NativeSculptVertexInfluence{.boneName = "hand_l", .weight = 1.0f},
            };
            if (ri::scene::ScaleNativeSculptBoneWeights(paint, "hand_l", 0.5f) == 0U
                || paint.vertexInfluences.front().size() != 1U
                || std::abs(paint.vertexInfluences.front().front().weight - 0.5f) > 0.001f) {
                std::cerr << "Half-weight on rigid vert did not leave unbound 0.5.\n";
                return EXIT_FAILURE;
            }
            if (ri::scene::ScaleNativeSculptBoneWeights(paint, "hand_l", 2.0f) == 0U
                || std::abs(paint.vertexInfluences.front().front().weight - 1.0f) > 0.001f) {
                std::cerr << "Double-weight did not restore rigid hand_l to 1.0.\n";
                return EXIT_FAILURE;
            }
            if (ri::scene::ScaleNativeSculptBoneWeights(paint, "hand_l", 2.0f) != 0U) {
                std::cerr << "Double-weight on already-rigid 1.0 reported work.\n";
                return EXIT_FAILURE;
            }
        }
        // 1-W on a balanced 0.5/0.5 blend must be a true no-op.
        {
            (void)ri::scene::FloodNativeSculptWeights(paint, "hips");
            paint.vertexBoneNames.front() = "hand_l";
            paint.vertexInfluences.front() = {
                ri::content::NativeSculptVertexInfluence{.boneName = "hand_l", .weight = 0.5f},
                ri::content::NativeSculptVertexInfluence{.boneName = "hips", .weight = 0.5f},
            };
            if (ri::scene::InvertNativeSculptBoneWeights(paint, "hand_l") != 0U) {
                std::cerr << "Invert on a 0.5/0.5 blend reported a false change.\n";
                return EXIT_FAILURE;
            }
        }
        paint.vertexBoneNames.front() = "hand_l";
        paint.vertexInfluences.front() = {
            ri::content::NativeSculptVertexInfluence{.boneName = "hand_l", .weight = 0.8f},
            ri::content::NativeSculptVertexInfluence{.boneName = "hips", .weight = 0.2f},
        };
        {
            const std::vector<float> clipped =
                ri::scene::ExtractNativeSculptBoneWeights(paint, "hand_l");
            if (clipped.size() != paint.mesh.positions.size() || clipped.front() < 0.65f) {
                std::cerr << "Extract bone weights missed hand_l peak.\n";
                return EXIT_FAILURE;
            }
            if (ri::scene::ApplyNativeSculptBoneWeights(paint, "hand_r", clipped) == 0U) {
                std::cerr << "Apply bone weights wrote no verts onto hand_r.\n";
                return EXIT_FAILURE;
            }
            const std::vector<float> pasted =
                ri::scene::ExtractNativeSculptBoneWeights(paint, "hand_r");
            if (pasted.empty() || pasted.front() < 0.65f) {
                std::cerr << "Paste bone weights did not restore hand_r peak.\n";
                return EXIT_FAILURE;
            }
        }
        if (ri::scene::SmoothNativeSculptWeights(paint) == 0U) {
            std::cerr << "Smooth weights changed no vertices.\n";
            return EXIT_FAILURE;
        }
        if (ri::scene::FloodNativeSculptWeights(paint, "hips") == 0U) {
            std::cerr << "Could not restore hips before grow/shrink.\n";
            return EXIT_FAILURE;
        }
        // Leave a seed on the first vert, grow onto neighbors, then shrink the frontier back.
        for (std::size_t vertex = 1; vertex < paint.vertexBoneNames.size(); ++vertex) {
            paint.vertexBoneNames[vertex].clear();
            if (vertex < paint.vertexInfluences.size()) {
                paint.vertexInfluences[vertex].clear();
            }
        }
        paint.vertexBoneNames.front() = "hips";
        if (!paint.vertexInfluences.empty()) {
            paint.vertexInfluences.front() = {{"hips", 1.0f}};
        }
        const std::size_t grown = ri::scene::GrowNativeSculptBoneWeights(paint, "hips");
        if (grown == 0U) {
            std::cerr << "Grow bone weights painted no neighbors.\n";
            return EXIT_FAILURE;
        }
        const std::size_t shrunk = ri::scene::ShrinkNativeSculptBoneWeights(paint, "hips");
        if (shrunk == 0U) {
            std::cerr << "Shrink bone weights cleared no frontier verts.\n";
            return EXIT_FAILURE;
        }
        (void)ri::scene::FloodNativeSculptWeights(paint, "hips");
        paint.vertexBoneNames.front() = "spine";
        paint.vertexInfluences.front() = {{"spine", 1.0f}};
        if (ri::scene::SmoothNativeSculptBoneWeights(paint, "spine") == 0U
            || paint.vertexInfluences.front().size() < 2U) {
            std::cerr << "Soft Bone did not blend spine toward hips neighbors.\n";
            return EXIT_FAILURE;
        }
        bool softHasSpine = false;
        bool softHasHips = false;
        for (const ri::content::NativeSculptVertexInfluence& influence : paint.vertexInfluences.front()) {
            if (influence.boneName == "spine") {
                softHasSpine = true;
            }
            if (influence.boneName == "hips") {
                softHasHips = true;
            }
        }
        if (!softHasSpine || !softHasHips) {
            std::cerr << "Soft Bone dropped spine or hips while softening.\n";
            return EXIT_FAILURE;
        }
        // Blend a soft hand_l, then harden verts at/above 0.5 into a rigid bind.
        (void)ri::scene::FloodNativeSculptWeights(paint, "hips");
        paint.vertexBoneNames.front() = "hand_l";
        if (!paint.vertexInfluences.empty()) {
            paint.vertexInfluences.front() = {
                ri::content::NativeSculptVertexInfluence{.boneName = "hand_l", .weight = 0.7f},
                ri::content::NativeSculptVertexInfluence{.boneName = "hips", .weight = 0.3f},
            };
        }
        if (ri::scene::HardenNativeSculptBoneWeights(paint, "hand_l", 0.5f) == 0U
            || paint.vertexBoneNames.front() != "hand_l"
            || paint.vertexInfluences.front().size() != 1U
            || paint.vertexInfluences.front().front().weight < 0.999f) {
            std::cerr << "Harden bone weights did not snap the blended vert.\n";
            return EXIT_FAILURE;
        }
        if (ri::scene::HardenNativeSculptBoneWeights(paint, "hand_l", 0.5f) != 0U) {
            std::cerr << "Harden reported work on an already-rigid vert.\n";
            return EXIT_FAILURE;
        }
        paint.vertexBoneNames.front() = "hand_l";
        paint.vertexInfluences.front() = {
            ri::content::NativeSculptVertexInfluence{.boneName = "hand_l", .weight = 0.03f},
            ri::content::NativeSculptVertexInfluence{.boneName = "hips", .weight = 0.97f},
        };
        if (ri::scene::FloorNativeSculptBoneWeights(paint, "hand_l", 0.05f) == 0U
            || paint.vertexBoneNames.front() != "hips"
            || paint.vertexInfluences.front().size() != 1U) {
            std::cerr << "Floor bone weights did not drop the weak hand_l influence.\n";
            return EXIT_FAILURE;
        }
        if (ri::scene::FloorNativeSculptBoneWeights(paint, "hips", 0.05f) != 0U) {
            std::cerr << "Floor reported work on a strong hips influence.\n";
            return EXIT_FAILURE;
        }
        paint.vertexBoneNames.front() = "hand_l";
        paint.vertexInfluences.front() = {
            ri::content::NativeSculptVertexInfluence{.boneName = "hand_l", .weight = 0.92f},
            ri::content::NativeSculptVertexInfluence{.boneName = "hips", .weight = 0.08f},
        };
        if (ri::scene::CeilNativeSculptBoneWeights(paint, "hand_l", 0.85f) == 0U) {
            std::cerr << "Ceil bone weights did not cap the strong hand_l influence.\n";
            return EXIT_FAILURE;
        }
        {
            float handWeight = 0.0f;
            for (const ri::content::NativeSculptVertexInfluence& influence :
                 paint.vertexInfluences.front()) {
                if (influence.boneName == "hand_l") {
                    handWeight = influence.weight;
                }
            }
            if (handWeight > 0.86f) {
                std::cerr << "Ceil left hand_l above the ceiling.\n";
                return EXIT_FAILURE;
            }
        }
        // Solo rigid above Ceil should leave unbound remainder, not no-op.
        paint.vertexBoneNames.front() = "hand_l";
        paint.vertexInfluences.front() = {
            ri::content::NativeSculptVertexInfluence{.boneName = "hand_l", .weight = 1.0f},
        };
        if (ri::scene::CeilNativeSculptBoneWeights(paint, "hand_l", 0.75f) == 0U
            || paint.vertexInfluences.front().size() != 1U
            || paint.vertexInfluences.front().front().weight > 0.76f
            || paint.vertexInfluences.front().front().weight < 0.74f) {
            std::cerr << "Ceil solo did not leave hand_l at the ceiling with unbound remainder.\n";
            return EXIT_FAILURE;
        }
        // Paste onto a full Cap4 slot: a strong new bone must replace the weakest.
        {
            paint.vertexBoneNames.front() = "hips";
            paint.vertexInfluences.front() = {
                {"hips", 0.40f},
                {"spine", 0.30f},
                {"chest", 0.20f},
                {"neck", 0.10f},
            };
            std::vector<float> paste(paint.mesh.positions.size(), 0.0f);
            paste.front() = 0.55f;
            if (ri::scene::ApplyNativeSculptBoneWeights(paint, "hand_r", paste) == 0U) {
                std::cerr << "Cap-safe paste wrote no verts onto a full influence list.\n";
                return EXIT_FAILURE;
            }
            bool hasHand = false;
            bool hasNeck = false;
            for (const ri::content::NativeSculptVertexInfluence& influence :
                 paint.vertexInfluences.front()) {
                if (influence.boneName == "hand_r") {
                    hasHand = true;
                }
                if (influence.boneName == "neck") {
                    hasNeck = true;
                }
            }
            if (!hasHand || hasNeck
                || paint.vertexInfluences.front().size()
                    > static_cast<std::size_t>(ri::content::NativeSculptDocument::kMaxInfluences)) {
                std::cerr << "Cap-safe paste did not replace the weakest influence.\n";
                return EXIT_FAILURE;
            }
        }
        // Transfer onto an over-cap list must claim a slot for the destination bone.
        {
            paint.vertexBoneNames.front() = "hips";
            paint.vertexInfluences.front() = {
                {"hips", 0.30f},
                {"spine", 0.25f},
                {"chest", 0.20f},
                {"neck", 0.15f},
                {"hand_l", 0.10f},
            };
            if (ri::scene::TransferNativeSculptWeights(paint, "hand_l", "hand_r") == 0U) {
                std::cerr << "Cap-safe transfer wrote no verts.\n";
                return EXIT_FAILURE;
            }
            bool hasHandR = false;
            bool hasHandL = false;
            for (const ri::content::NativeSculptVertexInfluence& influence :
                 paint.vertexInfluences.front()) {
                if (influence.boneName == "hand_r") {
                    hasHandR = true;
                }
                if (influence.boneName == "hand_l") {
                    hasHandL = true;
                }
            }
            if (!hasHandR || hasHandL
                || paint.vertexInfluences.front().size()
                    > static_cast<std::size_t>(ri::content::NativeSculptDocument::kMaxInfluences)) {
                std::cerr << "Cap-safe transfer did not land hand_r within Cap4.\n";
                return EXIT_FAILURE;
            }
        }
        // Blend-paint Cap4: a mid-strength dab must be able to claim the weakest slot.
        {
            paint.vertexBoneNames.front() = "hips";
            paint.vertexInfluences.front() = {
                {"hips", 0.40f},
                {"spine", 0.30f},
                {"chest", 0.20f},
                {"neck", 0.10f},
            };
            const ri::math::Vec3 center = paint.mesh.positions.front();
            const std::size_t painted = ri::scene::PaintNativeSculptWeights(
                paint,
                center,
                2.0f,
                "hand_r",
                ri::scene::NativeSculptWeightPaint::Add,
                false,
                false,
                false,
                0.35f);
            if (painted == 0U) {
                std::cerr << "Cap4 blend paint claimed no verts with a mid-strength dab.\n";
                return EXIT_FAILURE;
            }
            bool hasHand = false;
            for (const ri::content::NativeSculptVertexInfluence& influence :
                 paint.vertexInfluences.front()) {
                if (influence.boneName == "hand_r") {
                    hasHand = true;
                }
            }
            if (!hasHand) {
                std::cerr << "Cap4 blend paint did not introduce hand_r.\n";
                return EXIT_FAILURE;
            }
        }
        // Stuff five influences, then Cap4 should drop the weakest and renormalize.
        paint.vertexBoneNames.front() = "hips";
        paint.vertexInfluences.front() = {
            {"hips", 0.40f},
            {"spine", 0.25f},
            {"chest", 0.15f},
            {"neck", 0.12f},
            {"head", 0.08f},
        };
        if (ri::scene::AuditNativeSculptWeights(paint).overInfluencedCount == 0U) {
            std::cerr << "Audit missed an over-cap influence list.\n";
            return EXIT_FAILURE;
        }
        if (ri::scene::LimitNativeSculptInfluences(paint, 4) == 0U
            || paint.vertexInfluences.front().size() != 4U
            || paint.vertexInfluences.front().back().boneName == "head") {
            std::cerr << "Limit influences did not keep the strongest four.\n";
            return EXIT_FAILURE;
        }
        if (ri::scene::AuditNativeSculptWeights(paint).overInfluencedCount != 0U) {
            std::cerr << "Audit still reports over-cap after Cap4.\n";
            return EXIT_FAILURE;
        }
        paint.vertexBoneNames.front() = "ghost_fin";
        paint.vertexInfluences.front() = {
            {"ghost_fin", 0.6f},
            {"hips", 0.4f},
        };
        const std::vector<std::string> allowedBones{"hips", "spine", "chest"};
        if (ri::scene::StripNativeSculptOrphanInfluences(paint, allowedBones) == 0U
            || paint.vertexBoneNames.front() != "hips"
            || paint.vertexInfluences.front().size() != 1U
            || paint.vertexInfluences.front().front().boneName != "hips") {
            std::cerr << "Strip orphan influences did not drop ghost_fin.\n";
            return EXIT_FAILURE;
        }
        // Leave a hips seed and clear the rest, then Seal should pull neighbor weights in.
        (void)ri::scene::FloodNativeSculptWeights(paint, "hips");
        for (std::size_t vertex = 1; vertex < paint.vertexBoneNames.size(); ++vertex) {
            paint.vertexBoneNames[vertex].clear();
            if (vertex < paint.vertexInfluences.size()) {
                paint.vertexInfluences[vertex].clear();
            }
        }
        const std::size_t sealed = ri::scene::SealUnboundNativeSculptWeightsFromNeighbors(paint);
        if (sealed == 0U) {
            std::cerr << "Seal unbound weights filled no neighbor verts.\n";
            return EXIT_FAILURE;
        }
        bool sealedNeighbor = false;
        for (std::size_t vertex = 1; vertex < paint.vertexBoneNames.size(); ++vertex) {
            if (!paint.vertexBoneNames[vertex].empty()
                || (vertex < paint.vertexInfluences.size() && !paint.vertexInfluences[vertex].empty())) {
                sealedNeighbor = true;
                break;
            }
        }
        if (!sealedNeighbor) {
            std::cerr << "Seal unbound weights did not bind any neighbor of hips.\n";
            return EXIT_FAILURE;
        }
        // Ensure a full hips flood so the blend-paint checks below stay deterministic.
        (void)ri::scene::FloodNativeSculptWeights(paint, "hips");
    if (paint.vertexBoneNames.front() != "hips"
        || std::any_of(
            paint.vertexBoneNames.begin(),
            paint.vertexBoneNames.end(),
            [](const std::string& boneName) { return boneName != "hips"; })) {
        std::cerr << "Could not restore hips flood after unbound test.\n";
        return EXIT_FAILURE;
    }
    const std::size_t added = ri::scene::PaintNativeSculptWeights(
        paint,
        paint.mesh.positions.front(),
        0.35f,
        "spine",
        ri::scene::NativeSculptWeightPaint::Add,
        false,
        false,
        false,
        1.0f);
    if (added == 0U
        || paint.vertexInfluences.size() != paint.mesh.positions.size()
        || paint.vertexInfluences.front().size() < 2U) {
        std::cerr << "Add paint did not blend a second influence.\n";
        return EXIT_FAILURE;
    }
    bool hasHips = false;
    bool hasSpine = false;
    for (const ri::content::NativeSculptVertexInfluence& influence : paint.vertexInfluences.front()) {
        if (influence.boneName == "hips") {
            hasHips = true;
        }
        if (influence.boneName == "spine") {
            hasSpine = true;
        }
    }
    if (!hasHips || !hasSpine) {
        std::cerr << "Add paint dropped an existing influence.\n";
        return EXIT_FAILURE;
    }
    const char* extraBones[] = {"chest", "neck", "head", "left_upper_arm"};
    for (const char* extra : extraBones) {
        (void)ri::scene::PaintNativeSculptWeights(
            paint,
            paint.mesh.positions.front(),
            0.35f,
            extra,
            ri::scene::NativeSculptWeightPaint::Add,
            false,
            false,
            false,
            1.0f);
    }
    if (paint.vertexInfluences.front().size() > 4U) {
        std::cerr << "Add paint kept more than four influences.\n";
        return EXIT_FAILURE;
    }
    if (ri::scene::FloodNativeSculptWeights(paint, "hips") == 0U) {
        std::cerr << "Could not restore hips after blend paint.\n";
        return EXIT_FAILURE;
    }
    paint.vertexBoneNames.front() = "outlier";
    const std::size_t smoothed = ri::scene::PaintNativeSculptWeights(
        paint, paint.mesh.positions.front(), 0.6f, "", ri::scene::NativeSculptWeightPaint::Smooth);
    if (smoothed == 0U || paint.vertexBoneNames.front() != "hips") {
        std::cerr << "Smooth paint did not pull the outlier vertex onto its neighbors.\n";
        return EXIT_FAILURE;
    }

    ri::content::NativeSculptDocument block =
        ri::scene::CreateNativeSculptDocument("paint_block", "Paint Block", ri::scene::NativeSculptCage::Cube, 8, 8);
    if (!ri::scene::BindNativeSculptToRig(block, humanoid, "rigs/bind_fixture.ri_rig.json").valid
        || ri::scene::FloodNativeSculptWeights(block, "left_hand") == 0U) {
        std::cerr << "Could not flood a cube for extrude weight inherit.\n";
        return EXIT_FAILURE;
    }
    const std::size_t blockVerts = block.mesh.positions.size();
    if (!ri::scene::ApplyNativeSculptFaceExtrude(
            block.mesh,
            ri::scene::NativeSculptStroke{
                .worldPosition = {0.0f, 0.5f, 0.0f},
                .worldNormal = {0.0f, 1.0f, 0.0f},
                .radius = 0.35f,
                .strength = 0.12f,
                .brush = ri::scene::NativeSculptBrush::Extrude,
            },
            &block.vertexBoneNames)
        || block.mesh.positions.size() <= blockVerts
        || block.vertexBoneNames.size() != block.mesh.positions.size()) {
        std::cerr << "Extrude did not inherit painted weights onto new vertices.\n";
        return EXIT_FAILURE;
    }
    for (const std::string& boneName : block.vertexBoneNames) {
        if (boneName != "left_hand") {
            std::cerr << "Extruded cap vertex lost its source bone.\n";
            return EXIT_FAILURE;
        }
    }
    if (!ri::scene::FloodNativeSculptWeights(paint, "left_hand")) {
        std::cerr << "Could not flood left_hand for keep-clear.\n";
        return EXIT_FAILURE;
    }
    paint.vertexBoneNames.front().clear();
    if (!ri::scene::BindNativeSculptToRig(paint, humanoid, "rigs/bind_fixture.ri_rig.json").valid
        || !paint.vertexBoneNames.front().empty()
        || paint.vertexBoneNames[1] != "left_hand") {
        std::cerr << "Rebind filled a cleared vertex or lost neighboring paint.\n";
        return EXIT_FAILURE;
    }

    ri::content::NativeSculptDocument maintain =
        ri::scene::CreateNativeSculptDocument("maintain", "Maintain", ri::scene::NativeSculptCage::Sphere, 16, 8);
    if (!ri::scene::BindNativeSculptToRig(maintain, humanoid, "rigs/bind_fixture.ri_rig.json").valid) {
        std::cerr << "Could not bind the maintain sculpt.\n";
        return EXIT_FAILURE;
    }
    maintain.vertexInfluences[0] = {
        ri::content::NativeSculptVertexInfluence{.boneName = "left_hand", .weight = 0.35f},
        ri::content::NativeSculptVertexInfluence{.boneName = "spine", .weight = 0.35f},
    };
    maintain.vertexBoneNames[0] = "left_hand";
    const ri::scene::NativeSculptWeightAudit auditBefore =
        ri::scene::AuditNativeSculptWeights(maintain);
    if (auditBefore.nonNormalizedCount == 0U) {
        std::cerr << "Audit did not flag non-normalized weights.\n";
        return EXIT_FAILURE;
    }
    if (ri::scene::NormalizeAllNativeSculptWeights(maintain) == 0U
        || ri::scene::AuditNativeSculptWeights(maintain).nonNormalizedCount != 0U) {
        std::cerr << "Normalize all did not repair vertex weights.\n";
        return EXIT_FAILURE;
    }
    maintain.vertexInfluences[0].push_back(
        ri::content::NativeSculptVertexInfluence{.boneName = "chest", .weight = 0.03f});
    if (ri::scene::PruneAllNativeSculptWeights(maintain, 0.05f) == 0U
        || maintain.vertexInfluences[0].size() != 2U) {
        std::cerr << "Prune all did not drop tiny influences.\n";
        return EXIT_FAILURE;
    }
    if (ri::scene::FloodNativeSculptWeights(maintain, "left_hand") == 0U) {
        std::cerr << "Could not flood maintain sculpt for mirror.\n";
        return EXIT_FAILURE;
    }
    std::size_t plusLeft = 0;
    std::size_t minusRight = 0;
    for (std::size_t vertex = 0; vertex < maintain.mesh.positions.size(); ++vertex) {
        if (maintain.mesh.positions[vertex].x > 0.05f && maintain.vertexBoneNames[vertex] == "left_hand") {
            ++plusLeft;
        }
        if (maintain.mesh.positions[vertex].x < -0.05f && maintain.vertexBoneNames[vertex] == "right_hand") {
            ++minusRight;
        }
    }
    if (plusLeft == 0U) {
        std::cerr << "Maintain mirror setup had no +X left-hand verts.\n";
        return EXIT_FAILURE;
    }
    if (ri::scene::MirrorNativeSculptWeights(maintain, true) == 0U) {
        std::cerr << "Document weight mirror changed no vertices.\n";
        return EXIT_FAILURE;
    }
    minusRight = 0;
    for (std::size_t vertex = 0; vertex < maintain.mesh.positions.size(); ++vertex) {
        if (maintain.mesh.positions[vertex].x < -0.05f && maintain.vertexBoneNames[vertex] == "right_hand") {
            ++minusRight;
        }
    }
    if (minusRight == 0U) {
        std::cerr << "Document weight mirror did not write right_hand on -X verts.\n";
        return EXIT_FAILURE;
    }

    if (ri::scene::RenameNativeSculptBone(maintain, "right_hand", "right_grip") == 0U
        || std::any_of(
            maintain.vertexBoneNames.begin(),
            maintain.vertexBoneNames.end(),
            [](const std::string& boneName) { return boneName == "right_hand"; })) {
        std::cerr << "Sculpt bone rename did not update bound vertex names.\n";
        return EXIT_FAILURE;
    }
    if (ri::scene::RemoveNativeSculptBone(maintain, "right_grip") == 0U
        || std::any_of(
            maintain.vertexBoneNames.begin(),
            maintain.vertexBoneNames.end(),
            [](const std::string& boneName) { return boneName == "right_grip"; })) {
        std::cerr << "Sculpt bone remove did not clear bound vertex names.\n";
        return EXIT_FAILURE;
    }

    if (ri::scene::FloodNativeSculptWeights(maintain, "left_hand") == 0U) {
        std::cerr << "Flood after remove left no verts.\n";
        return EXIT_FAILURE;
    }
    maintain.rigPath = "rigs/humanoid.ri_rig.json";
    if (ri::scene::ClearNativeSculptWeights(maintain) == 0U
        || ri::scene::AuditNativeSculptWeights(maintain).boundCount != 0U) {
        std::cerr << "Clear weights left bound vertices.\n";
        return EXIT_FAILURE;
    }
    maintain.rigPath = "rigs/humanoid.ri_rig.json";
    if (ri::scene::FloodNativeSculptWeights(maintain, "left_hand") == 0U) {
        std::cerr << "Flood before unbind left no verts.\n";
        return EXIT_FAILURE;
    }
    if (ri::scene::UnbindNativeSculptFromRig(maintain) == 0U || !maintain.rigPath.empty()
        || ri::scene::AuditNativeSculptWeights(maintain).boundCount != 0U) {
        std::cerr << "Unbind did not clear rig path and weights.\n";
        return EXIT_FAILURE;
    }

    ri::content::BlockCharacterDocument psxParts{};
    psxParts.id = "psx_scout";
    psxParts.displayName = "PSX Scout";
    psxParts.parts = {
        ri::content::BlockCharacterPart{
            .id = "head",
            .name = "Head",
            .boneName = "head",
            .shape = "dome",
            .center = {0.0F, 1.98F, 0.04F},
            .halfExtent = {0.32F, 0.30F, 0.30F},
            .albedoColor = {0.48F, 0.58F, 0.36F},
        },
        ri::content::BlockCharacterPart{
            .id = "chest",
            .name = "Chest",
            .boneName = "chest",
            .shape = "prism",
            .center = {0.0F, 1.40F, 0.04F},
            .halfExtent = {0.36F, 0.24F, 0.16F},
            .halfExtentTop = {0.26F, 0.24F, 0.12F},
            .albedoColor = {0.58F, 0.66F, 0.38F},
        },
        ri::content::BlockCharacterPart{
            .id = "boot_l",
            .name = "Boot L",
            .boneName = "left_foot",
            .shape = "wedge",
            .center = {-0.15F, 0.02F, 0.14F},
            .halfExtent = {0.14F, 0.08F, 0.26F},
            .albedoColor = {0.30F, 0.26F, 0.20F},
        },
    };
    ri::content::NativeSculptDocument psx =
        ri::scene::CreateBlockCharacterSculptDocument("psx scout", "PSX Scout", psxParts);
    const auto psxReport = ri::content::ValidateNativeSculptDocument(psx);
    if (!psxReport.valid || psx.cage != "psx" || psx.mesh.positions.size() < 24U * 5U
        || psx.vertexBoneNames.size() != psx.mesh.positions.size()
        || psx.vertexInfluences.size() != psx.mesh.positions.size()
        || psx.mesh.normals.size() != psx.mesh.positions.size()) {
        std::cerr << "PSX humanoid sculpt was incomplete.\n";
        return EXIT_FAILURE;
    }
    const ri::scene::RigDefinition psxRig =
        ri::scene::CreateHumanoidRigDefinition("psx_scout", "PSX Scout");
    const auto psxBind =
        ri::scene::BindNativeSculptToRig(psx, psxRig, "rigs/psx_scout.ri_rig.json", false);
    if (!psxBind.valid || psx.rigPath.empty()) {
        std::cerr << "PSX bind failed: " << psxBind.summary << "\n";
        return EXIT_FAILURE;
    }
    const auto psxAudit = ri::scene::AuditNativeSculptWeights(psx);
    if (psxAudit.boundCount == 0U || psxAudit.unboundCount != 0U) {
        std::cerr << "PSX weights incomplete after bind.\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
