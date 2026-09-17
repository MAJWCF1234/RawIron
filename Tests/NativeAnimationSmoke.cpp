#include "RawIron/Scene/NativeAnimation.h"
#include "RawIron/Scene/RigAuthoring.h"
#include "RawIron/Scene/Scene.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
    const ri::scene::RigDefinition rig = ri::scene::CreateHumanoidRigDefinition("humanoid", "Humanoid");
    ri::content::NativeAnimationDocument clip =
        ri::content::CreateNativeAnimationDocument("wave", "Wave", "humanoid.ri_rig.json");
    ri::scene::SeedNativeAnimationFromRig(clip, rig);
    if (clip.tracks.size() != rig.bones.size()) {
        std::cerr << "Rig seed missed bones.\n";
        return EXIT_FAILURE;
    }

    ri::scene::Scene scene{"AnimPreview"};
    std::vector<int> boneNodes;
    boneNodes.reserve(rig.bones.size());
    for (const ri::scene::RigBone& bone : rig.bones) {
        const int node = scene.CreateNode(bone.name);
        scene.GetNode(node).localTransform = bone.restLocal;
        boneNodes.push_back(node);
    }

    const int firstNode = boneNodes.front();
    scene.GetNode(firstNode).localTransform.rotationDegrees.y = 25.0F;
    ri::scene::CaptureNativeAnimationPose(clip, scene, boneNodes, 0.5);
    const ri::scene::AnimationClip bound = ri::scene::BindNativeAnimationClip(clip, scene, boneNodes);
    if (bound.nodeTracks.size() != rig.bones.size()) {
        std::cerr << "Animation bind missed bone tracks.\n";
        return EXIT_FAILURE;
    }
    scene.GetNode(firstNode).localTransform.rotationDegrees.y = 0.0F;
    ri::scene::ApplyAnimationClip(scene, bound, 0.5);
    if (std::abs(scene.GetNode(firstNode).localTransform.rotationDegrees.y - 25.0F) > 0.001F) {
        std::cerr << "Bound clip did not sample the captured pose.\n";
        return EXIT_FAILURE;
    }
    if (ri::scene::FindNativeAnimationBoneNode(scene, boneNodes, rig.bones.front().name) != firstNode) {
        std::cerr << "Bone name lookup missed the first deform node.\n";
        return EXIT_FAILURE;
    }
    if (!ri::scene::SetNativeAnimationDuration(clip, 2.0) || clip.durationSeconds != 2.0
        || ri::scene::SetNativeAnimationDuration(clip, 0.0)
        || ri::scene::SetNativeAnimationDuration(clip, 601.0)) {
        std::cerr << "Duration set did not accept 2s and reject empty/overlong values.\n";
        return EXIT_FAILURE;
    }
    ri::scene::UpsertNativeAnimationKey(
        clip, scene.GetNode(firstNode).name, 1.5, scene.GetNode(firstNode).localTransform);
    if (!ri::scene::UpsertNativeAnimationEvent(clip, 0.25, "foot_l")
        || !ri::scene::UpsertNativeAnimationEvent(clip, 1.5, "late")
        || !ri::scene::UpsertNativeAnimationEvent(clip, 0.25, "foot_l")
        || clip.events.size() != 2U
        || ri::scene::UpsertNativeAnimationEvent(clip, 0.1, "   ")
        || !ri::scene::RemoveNativeAnimationEvent(clip, 1U)
        || clip.events.size() != 1U
        || clip.events.front().name != "foot_l") {
        std::cerr << "Event upsert/remove did not keep a unique named marker.\n";
        return EXIT_FAILURE;
    }
    if (!ri::scene::UpsertNativeAnimationEvent(clip, 1.5, "late")) {
        std::cerr << "Could not restore the late event for trim.\n";
        return EXIT_FAILURE;
    }
    const ri::scene::NativeAnimationTrimResult trimmed = ri::scene::TrimNativeAnimation(clip, 0.0, 1.0, false);
    if (!trimmed.valid || trimmed.removedKeys == 0U || trimmed.removedEvents != 1U
        || clip.durationSeconds != 1.0 || clip.events.size() != 1U
        || clip.events.front().name != "foot_l") {
        std::cerr << "Trim did not drop keys past the new end.\n";
        return EXIT_FAILURE;
    }
    bool stillHasLateKey = false;
    for (const auto& track : clip.tracks) {
        for (const auto& key : track.keys) {
            if (key.timeSeconds > 1.0 + 1.0 / 120.0) {
                stillHasLateKey = true;
            }
        }
    }
    if (stillHasLateKey) {
        std::cerr << "Trim left a key past 1s.\n";
        return EXIT_FAILURE;
    }
    ri::content::NativeAnimationDocument window =
        ri::content::CreateNativeAnimationDocument("window", "Window", "humanoid.ri_rig.json");
    window.tracks.push_back(ri::content::NativeAnimationTrack{
        .boneName = "hips",
        .keys = {
            {.timeSeconds = 0.0, .scale = {1.0F, 1.0F, 1.0F}},
            {.timeSeconds = 0.5, .translation = {0.4F, 0.0F, 0.0F}, .scale = {1.0F, 1.0F, 1.0F}},
            {.timeSeconds = 1.2, .scale = {1.0F, 1.0F, 1.0F}},
        },
    });
    window.events = {
        {.timeSeconds = 0.1, .name = "before"},
        {.timeSeconds = 0.5, .name = "inside"},
        {.timeSeconds = 1.0, .name = "after"},
    };
    const ri::scene::NativeAnimationTrimResult shifted =
        ri::scene::TrimNativeAnimation(window, 0.25, 0.75, true);
    if (!shifted.valid || shifted.removedKeys != 2U || shifted.removedEvents != 2U
        || window.durationSeconds < 0.49
        || window.durationSeconds > 0.51 || window.tracks.front().keys.size() != 1U
        || std::abs(window.tracks.front().keys.front().timeSeconds - 0.25) > 0.001
        || window.events.size() != 1U || window.events.front().name != "inside"
        || std::abs(window.events.front().timeSeconds - 0.25) > 0.001) {
        std::cerr << "Window trim did not shift the kept key to local time.\n";
        return EXIT_FAILURE;
    }
    clip.tracks.push_back(ri::content::NativeAnimationTrack{.boneName = "missing_wing"});
    const ri::scene::NativeAnimationBindReport matched =
        ri::scene::DiagnoseNativeAnimationBind(clip, scene, boneNodes);
    if (matched.missingBoneCount == 0U || matched.boundTrackCount == 0U
        || matched.summary.find("missing_wing") == std::string::npos) {
        std::cerr << "Bind diagnose missed an unmatched track.\n";
        return EXIT_FAILURE;
    }
    const int secondNode = boneNodes[1];
    scene.GetNode(secondNode).localTransform.position.x = 0.4F;
    ri::scene::UpsertNativeAnimationKey(
        clip, scene.GetNode(secondNode).name, 0.5, scene.GetNode(secondNode).localTransform);
    scene.GetNode(secondNode).localTransform.position.x = 0.0F;
    const ri::scene::AnimationClip keyed =
        ri::scene::BindNativeAnimationClip(clip, scene, boneNodes);
    ri::scene::ApplyAnimationClip(scene, keyed, 0.5);
    if (std::abs(scene.GetNode(secondNode).localTransform.position.x - 0.4F) > 0.001F) {
        std::cerr << "Single-bone key did not sample on bind.\n";
        return EXIT_FAILURE;
    }
    if (std::abs(scene.GetNode(firstNode).localTransform.rotationDegrees.y - 25.0F) > 0.001F) {
        std::cerr << "Single-bone key overwrote the rest of the pose.\n";
        return EXIT_FAILURE;
    }
    if (!ri::scene::RemoveNativeAnimationKey(clip, scene.GetNode(secondNode).name, 0.5)
        || ri::scene::RemoveNativeAnimationKey(clip, scene.GetNode(secondNode).name, 0.5)) {
        std::cerr << "Single-bone key delete did not remove the keyed sample once.\n";
        return EXIT_FAILURE;
    }
    {
        const auto prev = ri::scene::FindNearestNativeAnimationKeyTime(clip, 0.5, true);
        const auto next = ri::scene::FindNearestNativeAnimationKeyTime(clip, 0.0, false);
        const auto closest = ri::scene::FindClosestNativeAnimationKeyTime(clip, 0.24);
        if (!prev.has_value() || *prev > 0.01
            || !next.has_value() || *next < 0.2
            || !closest.has_value() || *closest > 0.01) {
            std::cerr << "Nearest/closest key lookup missed rest/capture times.\n";
            return EXIT_FAILURE;
        }
    }
    if (ri::scene::InsertNativeAnimationRestKeys(clip, rig, 0.75, "left_hand") != 1U) {
        std::cerr << "Rest key insert for one bone failed.\n";
        return EXIT_FAILURE;
    }
    {
        ri::scene::Transform leftPose{
            .position = {0.2f, 0.0f, 0.0f},
            .rotationDegrees = {0.0f, 15.0f, 0.0f},
        };
        ri::scene::UpsertNativeAnimationKey(clip, "left_hand", 0.8, leftPose);
        if (ri::scene::MirrorNativeAnimationTrackAcrossX(clip, "left_hand") == 0U) {
            std::cerr << "Mirror animation keys across X failed.\n";
            return EXIT_FAILURE;
        }
        bool foundPartner = false;
        for (const ri::content::NativeAnimationTrack& track : clip.tracks) {
            if (track.boneName != "right_hand") {
                continue;
            }
            for (const ri::content::NativeAnimationKeyframe& key : track.keys) {
                if (std::abs(key.timeSeconds - 0.8) > 1.0 / 120.0) {
                    continue;
                }
                if (key.translation.x > -0.1f) {
                    std::cerr << "Mirrored key did not flip X translation.\n";
                    return EXIT_FAILURE;
                }
                foundPartner = true;
                break;
            }
        }
        if (!foundPartner) {
            std::cerr << "Mirrored keys did not write a right_hand sample.\n";
            return EXIT_FAILURE;
        }
    }
    {
        const double beforeDuration = clip.durationSeconds;
        if (!ri::scene::ScaleNativeAnimationTime(clip, 2.0)
            || std::abs(clip.durationSeconds - beforeDuration * 2.0) > 0.01
            || !ri::scene::ScaleNativeAnimationTime(clip, 0.5)
            || std::abs(clip.durationSeconds - beforeDuration) > 0.01) {
            std::cerr << "Scale animation time failed to round-trip.\n";
            return EXIT_FAILURE;
        }
    }
    {
        ri::content::NativeAnimationDocument timed =
            ri::content::CreateNativeAnimationDocument("timed", "Timed", "humanoid.ri_rig.json");
        timed.durationSeconds = 2.0;
        ri::scene::UpsertNativeAnimationKey(
            timed, "left_hand", 0.4, ri::scene::Transform{.position = {0.1f, 0.0f, 0.0f}});
        ri::scene::UpsertNativeAnimationKey(
            timed, "left_hand", 1.2, ri::scene::Transform{.position = {0.2f, 0.0f, 0.0f}});
        if (!ri::scene::AlignNativeAnimationStart(timed)) {
            std::cerr << "Align animation start failed.\n";
            return EXIT_FAILURE;
        }
        bool hasZero = false;
        bool hasShifted = false;
        for (const ri::content::NativeAnimationTrack& track : timed.tracks) {
            for (const ri::content::NativeAnimationKeyframe& key : track.keys) {
                if (std::abs(key.timeSeconds) <= 1.0 / 120.0) {
                    hasZero = true;
                }
                if (std::abs(key.timeSeconds - 0.8) <= 1.0 / 120.0) {
                    hasShifted = true;
                }
            }
        }
        if (!hasZero || !hasShifted || std::abs(timed.durationSeconds - 1.6) > 0.05) {
            std::cerr << "Align did not shift keys/duration correctly.\n";
            return EXIT_FAILURE;
        }
        timed.durationSeconds = 5.0;
        if (!ri::scene::FitNativeAnimationDuration(timed)
            || std::abs(timed.durationSeconds - 0.8) > 0.05) {
            std::cerr << "Fit duration did not match the last key.\n";
            return EXIT_FAILURE;
        }
        if (!ri::scene::OffsetNativeAnimationTimes(timed, 0.1)
            || !ri::scene::OffsetNativeAnimationTimes(timed, -0.1)) {
            std::cerr << "Offset animation times failed.\n";
            return EXIT_FAILURE;
        }
        if (ri::scene::CopyNativeAnimationTrack(timed, "left_hand", "right_hand") == 0U) {
            std::cerr << "Copy animation track failed.\n";
            return EXIT_FAILURE;
        }
        bool copied = false;
        for (const ri::content::NativeAnimationTrack& track : timed.tracks) {
            if (track.boneName == "right_hand" && track.keys.size() >= 2U) {
                copied = true;
                break;
            }
        }
        if (!copied) {
            std::cerr << "Copy animation track did not create destination keys.\n";
            return EXIT_FAILURE;
        }
        if (!ri::scene::UpsertNativeAnimationEvent(timed, 0.2, "foot_plant")
            || !ri::scene::RenameNativeAnimationEvent(timed, 0, "foot_down")
            || timed.events.empty()
            || timed.events.front().name != "foot_down") {
            std::cerr << "Rename animation event failed.\n";
            return EXIT_FAILURE;
        }
        if (!ri::scene::SetNativeAnimationEventTime(timed, 0, 0.55)
            || timed.events.empty()
            || std::abs(timed.events.front().timeSeconds - 0.55) > 0.01) {
            std::cerr << "Set animation event time failed.\n";
            return EXIT_FAILURE;
        }
        if (!ri::scene::DuplicateNativeAnimationEventAt(timed, 0, 0.8)
            || timed.events.size() < 2U) {
            std::cerr << "Duplicate animation event failed.\n";
            return EXIT_FAILURE;
        }
        {
            ri::content::NativeAnimationDocument dup =
                ri::content::CreateNativeAnimationDocument("dup", "Dup", "humanoid.ri_rig.json");
            dup.durationSeconds = 1.0;
            const ri::scene::Transform pose{.position = {0.1f, 0.0f, 0.0f}};
            ri::scene::UpsertNativeAnimationKey(dup, "left_hand", 0.0, pose);
            ri::scene::UpsertNativeAnimationKey(dup, "left_hand", 0.1, pose);
            ri::scene::UpsertNativeAnimationKey(
                dup, "left_hand", 0.2, ri::scene::Transform{.position = {0.2f, 0.0f, 0.0f}});
            if (ri::scene::DeduplicateNativeAnimationKeys(dup) != 1U
                || dup.tracks.empty()
                || dup.tracks.front().keys.size() != 2U) {
                std::cerr << "Deduplicate animation keys failed.\n";
                return EXIT_FAILURE;
            }
        }
        {
            ri::content::NativeAnimationDocument grid =
                ri::content::CreateNativeAnimationDocument("grid", "Grid", "humanoid.ri_rig.json");
            grid.durationSeconds = 1.0;
            ri::scene::UpsertNativeAnimationKey(
                grid, "left_hand", 0.11, ri::scene::Transform{.position = {0.1f, 0.0f, 0.0f}});
            if (ri::scene::QuantizeNativeAnimationTimes(grid, 30.0) == 0U
                || grid.tracks.empty()
                || std::abs(grid.tracks.front().keys.front().timeSeconds - (3.0 / 30.0)) > 0.001) {
                std::cerr << "Quantize animation times failed.\n";
                return EXIT_FAILURE;
            }
        }
        if (ri::scene::ClearNativeAnimationEvents(timed) == 0U || !timed.events.empty()) {
            std::cerr << "Clear animation events failed.\n";
            return EXIT_FAILURE;
        }
        timed.tracks.push_back(ri::content::NativeAnimationTrack{.boneName = "ghost_fin"});
        ri::scene::UpsertNativeAnimationKey(
            timed, "ghost_fin", 0.1, ri::scene::Transform{.position = {1.0f, 0.0f, 0.0f}});
        if (ri::scene::StripNativeAnimationMissingTracks(timed, scene, boneNodes) == 0U
            || std::any_of(
                timed.tracks.begin(),
                timed.tracks.end(),
                [](const ri::content::NativeAnimationTrack& track) {
                    return track.boneName == "ghost_fin";
                })) {
            std::cerr << "Strip missing animation tracks failed.\n";
            return EXIT_FAILURE;
        }
        if (ri::scene::ClearNativeAnimationTrack(timed, "right_hand") == 0U
            || ri::scene::ClearNativeAnimationKeys(timed) == 0U
            || !timed.tracks.empty()) {
            std::cerr << "Clear animation track/keys failed.\n";
            return EXIT_FAILURE;
        }
    }
    scene.GetNode(secondNode).localTransform.position.x = 0.0F;
    const ri::scene::AnimationClip afterDelete =
        ri::scene::BindNativeAnimationClip(clip, scene, boneNodes);
    ri::scene::ApplyAnimationClip(scene, afterDelete, 0.5);
    if (std::abs(scene.GetNode(secondNode).localTransform.position.x) > 0.001F) {
        std::cerr << "Deleted bone key still sampled at 0.5s.\n";
        return EXIT_FAILURE;
    }
    const std::string secondBoneName = scene.GetNode(secondNode).name;
    if (!ri::scene::RenameNativeAnimationBone(clip, secondBoneName, "renamed_bone")
        || clip.tracks.back().boneName != "missing_wing") {
        std::cerr << "Animation bone rename missed the keyed track.\n";
        return EXIT_FAILURE;
    }
    if (std::none_of(
            clip.tracks.begin(),
            clip.tracks.end(),
            [](const ri::content::NativeAnimationTrack& track) {
                return track.boneName == "renamed_bone";
            })) {
        std::cerr << "Animation bone rename did not update the target track.\n";
        return EXIT_FAILURE;
    }
    const std::size_t tracksBeforeRemove = clip.tracks.size();
    if (!ri::scene::RemoveNativeAnimationBone(clip, "renamed_bone")
        || clip.tracks.size() != tracksBeforeRemove - 1U
        || std::any_of(
            clip.tracks.begin(),
            clip.tracks.end(),
            [](const ri::content::NativeAnimationTrack& track) {
                return track.boneName == "renamed_bone";
            })) {
        std::cerr << "Animation bone remove did not drop the target track.\n";
        return EXIT_FAILURE;
    }

    const int rootNode = ri::scene::FindNativeAnimationRootMotionNode(scene, boneNodes);
    if (rootNode != firstNode) {
        std::cerr << "Root-motion node was not the hierarchy root.\n";
        return EXIT_FAILURE;
    }
    const ri::scene::Transform restRoot = scene.GetNode(rootNode).localTransform;
    scene.GetNode(rootNode).localTransform.position.z = 2.0F;
    scene.GetNode(rootNode).localTransform.rotationDegrees.y = 15.0F;
    ri::scene::HoldNativeAnimationRootInPlace(scene, rootNode, restRoot);
    if (std::abs(scene.GetNode(rootNode).localTransform.position.z - restRoot.position.z) > 0.001F
        || std::abs(scene.GetNode(rootNode).localTransform.rotationDegrees.y - 15.0F) > 0.001F) {
        std::cerr << "In-place root hold did not restore translation or keep rotation.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
