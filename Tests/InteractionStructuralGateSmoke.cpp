#include "RawIron/Scene/InteractionStructuralGate.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/StructuralBrush.h"

#include <cstdlib>

int main() {
    ri::scene::Scene scene{"InteractionGate"};
    const int root = scene.CreateNode("Root");

    ri::scene::StructuralBrushSpawnOptions wall{};
    wall.nodeName = "BlockingWall";
    wall.parent = root;
    wall.structuralType = "box";
    wall.transform.position = {0.0f, 1.5f, 2.0f};
    wall.transform.scale = {2.0f, 3.0f, 0.4f};
    wall.metadata.brushId = "blocking_wall";
    wall.metadata.role = ri::scene::StructuralBrushSemanticRole::Wall;
    wall.metadata.queryMesh.raycastable = true;
    wall.metadata.queryMesh.traceable = true;
    wall.metadata.queryMesh.interactable = true;
    const int wallNode = ri::scene::AddStructuralBrushNode(scene, wall);
    if (wallNode == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }

    const ri::math::Vec3 origin{0.0f, 1.5f, 0.0f};
    const ri::math::Vec3 forward{0.0f, 0.0f, 1.0f};
    const ri::math::Vec3 unusedTarget{};

    // No target → gate skipped, still eligible.
    const auto noTarget =
        ri::scene::EvaluateInteractionStructuralGate(scene, origin, forward, unusedTarget, false);
    if (!noTarget.eligible || noTarget.evaluated || noTarget.blockedByStructural) {
        return EXIT_FAILURE;
    }

    // Target behind an interactable wall → blocked on look-ray t, not euclidean distance.
    const ri::math::Vec3 behindWall{0.0f, 1.5f, 6.0f};
    const auto blocked =
        ri::scene::EvaluateInteractionStructuralGate(scene, origin, forward, behindWall, true);
    if (!blocked.evaluated || !blocked.blockedByStructural || blocked.eligible
        || blocked.structuralHitDistance <= 0.0f || blocked.structuralHitDistance >= blocked.targetRayT
        || blocked.targetRayT < 5.9f) {
        return EXIT_FAILURE;
    }

    // Target closer than the wall along the look ray → eligible.
    const ri::math::Vec3 inFront{0.0f, 1.5f, 1.0f};
    const auto clear =
        ri::scene::EvaluateInteractionStructuralGate(scene, origin, forward, inFront, true);
    if (!clear.evaluated || clear.blockedByStructural || !clear.eligible || clear.targetRayT > 1.1f) {
        return EXIT_FAILURE;
    }

    // Overlap-style target beside/behind the camera must not raycast through the wall.
    const ri::math::Vec3 behindCamera{0.0f, 1.5f, -1.0f};
    const auto overlapOnly =
        ri::scene::EvaluateInteractionStructuralGate(scene, origin, forward, behindCamera, true);
    if (overlapOnly.evaluated || overlapOnly.blockedByStructural || !overlapOnly.eligible
        || overlapOnly.targetRayT >= 0.0f) {
        return EXIT_FAILURE;
    }

    // Off-axis target still blocked when the look ray hits the wall first.
    const ri::math::Vec3 offAxis{1.5f, 1.5f, 6.0f};
    const auto offAxisBlocked =
        ri::scene::EvaluateInteractionStructuralGate(scene, origin, forward, offAxis, true);
    if (!offAxisBlocked.evaluated || !offAxisBlocked.blockedByStructural || offAxisBlocked.eligible) {
        return EXIT_FAILURE;
    }

    // Restore a clearly occluding wall and confirm the far on-axis target stays blocked.
    scene.GetNode(wallNode).localTransform.position = {0.0f, 1.5f, 2.0f};
    scene.GetNode(wallNode).localTransform.scale = {2.0f, 3.0f, 0.4f};
    scene.GetNode(wallNode).structuralBrush.queryMesh.interactable = true;
    (void)scene.SetParent(wallNode, root);
    const auto stillBlocked =
        ri::scene::EvaluateInteractionStructuralGate(scene, origin, forward, behindWall, true);
    if (!stillBlocked.evaluated || !stillBlocked.blockedByStructural || stillBlocked.eligible) {
        return EXIT_FAILURE;
    }

    // The wall's own near face must not self-block when that node is the interaction target.
    const ri::math::Vec3 wallCenter{0.0f, 1.5f, 2.0f};
    const auto selfHit =
        ri::scene::EvaluateInteractionStructuralGate(scene, origin, forward, wallCenter, true);
    if (!selfHit.evaluated || !selfHit.blockedByStructural || selfHit.eligible) {
        return EXIT_FAILURE;
    }
    const auto selfIgnored = ri::scene::EvaluateInteractionStructuralGate(
        scene, origin, forward, wallCenter, true, wallNode);
    if (!selfIgnored.evaluated || selfIgnored.blockedByStructural || !selfIgnored.eligible) {
        return EXIT_FAILURE;
    }

    // Ignoring the far target's node must not hide an occluder in front.
    ri::scene::StructuralBrushSpawnOptions farTarget{};
    farTarget.nodeName = "FarTarget";
    farTarget.parent = root;
    farTarget.structuralType = "box";
    farTarget.transform.position = {0.0f, 1.5f, 6.0f};
    farTarget.transform.scale = {0.4f, 0.4f, 0.4f};
    farTarget.metadata.brushId = "far_target";
    farTarget.metadata.role = ri::scene::StructuralBrushSemanticRole::Decor;
    farTarget.metadata.queryMesh.raycastable = true;
    farTarget.metadata.queryMesh.traceable = true;
    farTarget.metadata.queryMesh.interactable = true;
    const int farNode = ri::scene::AddStructuralBrushNode(scene, farTarget);
    if (farNode == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }
    const auto ignoreFarStillBlocked = ri::scene::EvaluateInteractionStructuralGate(
        scene, origin, forward, behindWall, true, farNode);
    if (!ignoreFarStillBlocked.evaluated || !ignoreFarStillBlocked.blockedByStructural
        || ignoreFarStillBlocked.eligible) {
        return EXIT_FAILURE;
    }
    scene.GetNode(farNode).structuralBrush.queryMesh.interactable = false;

    // Non-interactable geometry must not block Interaction purpose.
    scene.GetNode(wallNode).structuralBrush.queryMesh.interactable = false;
    const auto ignored =
        ri::scene::EvaluateInteractionStructuralGate(scene, origin, forward, behindWall, true);
    if (!ignored.evaluated || ignored.blockedByStructural || !ignored.eligible) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
