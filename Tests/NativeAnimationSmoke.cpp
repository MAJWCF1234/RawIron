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
        ri::content::NativeAnimationDocument holdClip =
            ri::content::CreateNativeAnimationDocument("hold", "Hold", "humanoid.ri_rig.json");
        ri::scene::Transform heldPose{
            .position = {0.15f, 0.0f, 0.0f},
            .rotationDegrees = {0.0f, 10.0f, 0.0f},
        };
        ri::scene::UpsertNativeAnimationKey(holdClip, "left_hand", 0.5, heldPose);
        if (!ri::scene::HoldNativeAnimationKey(holdClip, "left_hand", 0.9)) {
            std::cerr << "Hold key failed with a previous sample present.\n";
            return EXIT_FAILURE;
        }
        bool held = false;
        for (const ri::content::NativeAnimationTrack& track : holdClip.tracks) {
            if (track.boneName != "left_hand") {
                continue;
            }
            for (const ri::content::NativeAnimationKeyframe& key : track.keys) {
                if (std::abs(key.timeSeconds - 0.9) > 1.0 / 120.0) {
                    continue;
                }
                if (std::abs(key.translation.x - 0.15f) > 0.001f) {
                    std::cerr << "Hold key did not repeat the previous translation.\n";
                    return EXIT_FAILURE;
                }
                held = true;
                break;
            }
        }
        if (!held) {
            std::cerr << "Hold key did not write a sample at 0.9s.\n";
            return EXIT_FAILURE;
        }
        if (ri::scene::HoldNativeAnimationKey(holdClip, "left_hand", 0.1)) {
            std::cerr << "Hold key should fail when no earlier key exists.\n";
            return EXIT_FAILURE;
        }
        ri::scene::UpsertNativeAnimationKey(
            holdClip, "right_hand", 0.4, ri::scene::Transform{.position = {-0.1f, 0.0f, 0.0f}});
        if (ri::scene::HoldNativeAnimationPose(holdClip, 1.0) < 2U) {
            std::cerr << "Hold pose did not cover both handed tracks.\n";
            return EXIT_FAILURE;
        }
        ri::content::NativeAnimationDocument bdClip =
            ri::content::CreateNativeAnimationDocument("bd", "Breakdown", "humanoid.ri_rig.json");
        ri::scene::UpsertNativeAnimationKey(
            bdClip, "left_hand", 0.0, ri::scene::Transform{.position = {0.0f, 0.0f, 0.0f}});
        ri::scene::UpsertNativeAnimationKey(
            bdClip, "left_hand", 1.0, ri::scene::Transform{.position = {0.4f, 0.0f, 0.0f}});
        if (!ri::scene::BreakdownNativeAnimationKey(bdClip, "left_hand", 0.5)) {
            std::cerr << "Breakdown key failed between surrounding samples.\n";
            return EXIT_FAILURE;
        }
        bool mid = false;
        for (const ri::content::NativeAnimationTrack& track : bdClip.tracks) {
            if (track.boneName != "left_hand") {
                continue;
            }
            for (const ri::content::NativeAnimationKeyframe& key : track.keys) {
                if (std::abs(key.timeSeconds - 0.5) > 1.0 / 120.0) {
                    continue;
                }
                if (std::abs(key.translation.x - 0.2f) > 0.001f) {
                    std::cerr << "Breakdown key did not land at the midpoint.\n";
                    return EXIT_FAILURE;
                }
                mid = true;
                break;
            }
        }
        if (!mid) {
            std::cerr << "Breakdown key did not write a mid sample.\n";
            return EXIT_FAILURE;
        }
        ri::scene::UpsertNativeAnimationKey(
            bdClip, "right_hand", 0.0, ri::scene::Transform{.position = {0.0f, 0.0f, 0.0f}});
        ri::scene::UpsertNativeAnimationKey(
            bdClip, "right_hand", 1.0, ri::scene::Transform{.position = {-0.4f, 0.0f, 0.0f}});
        if (ri::scene::BreakdownNativeAnimationPose(bdClip, 0.25) < 2U) {
            std::cerr << "Breakdown pose missed handed tracks.\n";
            return EXIT_FAILURE;
        }
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
        // Offset that piles keys onto the same snap frame must collapse them.
        {
            ri::content::NativeAnimationDocument piled = timed;
            for (ri::content::NativeAnimationTrack& track : piled.tracks) {
                if (track.boneName != "left_hand") {
                    continue;
                }
                track.keys = {
                    ri::content::NativeAnimationKeyframe{.timeSeconds = 0.10},
                    ri::content::NativeAnimationKeyframe{.timeSeconds = 0.101},
                    ri::content::NativeAnimationKeyframe{.timeSeconds = 0.50},
                };
                track.keys[0].translation = {1.0f, 0.0f, 0.0f};
                track.keys[1].translation = {9.0f, 0.0f, 0.0f};
                track.keys[2].translation = {3.0f, 0.0f, 0.0f};
            }
            if (!ri::scene::OffsetNativeAnimationBoneTimes(piled, "left_hand", -0.10)) {
                std::cerr << "Offset bone collapse setup failed.\n";
                return EXIT_FAILURE;
            }
            for (const ri::content::NativeAnimationTrack& track : piled.tracks) {
                if (track.boneName != "left_hand") {
                    continue;
                }
                if (track.keys.size() != 2U) {
                    std::cerr << "Offset did not collapse near-snap keys.\n";
                    return EXIT_FAILURE;
                }
                if (std::abs(track.keys.front().translation.x - 9.0f) > 0.001f) {
                    std::cerr << "Offset collapse did not keep the later sample.\n";
                    return EXIT_FAILURE;
                }
            }
        }
        double leftAt = -1.0;
        for (const ri::content::NativeAnimationTrack& track : timed.tracks) {
            if (track.boneName == "left_hand" && !track.keys.empty()) {
                leftAt = track.keys.front().timeSeconds;
                break;
            }
        }
        if (!ri::scene::OffsetNativeAnimationBoneTimes(timed, "left_hand", 0.2)
            || leftAt < 0.0) {
            std::cerr << "Offset bone animation times failed.\n";
            return EXIT_FAILURE;
        }
        for (const ri::content::NativeAnimationTrack& track : timed.tracks) {
            if (track.boneName != "left_hand" || track.keys.empty()) {
                continue;
            }
            if (std::abs(track.keys.front().timeSeconds - (leftAt + 0.2)) > 0.001) {
                std::cerr << "Offset bone times did not shift left_hand alone.\n";
                return EXIT_FAILURE;
            }
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
        // Paste/replace keys must not rewrite or resurrect the source track.
        {
            ri::content::NativeAnimationDocument pasteDoc = timed;
            std::vector<ri::content::NativeAnimationKeyframe> clipKeys{};
            for (const ri::content::NativeAnimationTrack& track : pasteDoc.tracks) {
                if (track.boneName == "left_hand") {
                    clipKeys = track.keys;
                    break;
                }
            }
            if (clipKeys.size() < 2U) {
                std::cerr << "Paste keys smoke missing left_hand clipboard.\n";
                return EXIT_FAILURE;
            }
            // Mutate source after copy, then delete it — paste must ignore that.
            for (ri::content::NativeAnimationTrack& track : pasteDoc.tracks) {
                if (track.boneName == "left_hand") {
                    track.keys.front().translation.x = 99.0f;
                }
            }
            pasteDoc.tracks.erase(
                std::remove_if(
                    pasteDoc.tracks.begin(),
                    pasteDoc.tracks.end(),
                    [](const ri::content::NativeAnimationTrack& track) {
                        return track.boneName == "left_hand";
                    }),
                pasteDoc.tracks.end());
            if (ri::scene::ReplaceNativeAnimationTrackKeys(pasteDoc, "spine", clipKeys) == 0U) {
                std::cerr << "Replace animation track keys failed.\n";
                return EXIT_FAILURE;
            }
            bool hasSpine = false;
            bool hasLeft = false;
            for (const ri::content::NativeAnimationTrack& track : pasteDoc.tracks) {
                if (track.boneName == "spine" && track.keys.size() == clipKeys.size()) {
                    hasSpine = true;
                    if (std::abs(track.keys.front().translation.x - 99.0f) < 0.001f) {
                        std::cerr << "Paste keys used mutated source instead of clipboard.\n";
                        return EXIT_FAILURE;
                    }
                }
                if (track.boneName == "left_hand") {
                    hasLeft = true;
                }
            }
            if (!hasSpine || hasLeft) {
                std::cerr << "Paste keys rewrote or resurrected the source track.\n";
                return EXIT_FAILURE;
            }
        }
        {
            ri::content::NativeAnimationDocument grid =
                ri::content::CreateNativeAnimationDocument("grid_collapse", "Grid", "humanoid.ri_rig.json");
            grid.durationSeconds = 1.0;
            ri::scene::UpsertNativeAnimationKey(
                grid, "left_hand", 0.10, ri::scene::Transform{.position = {1.0f, 0.0f, 0.0f}});
            ri::scene::UpsertNativeAnimationKey(
                grid, "left_hand", 0.11, ri::scene::Transform{.position = {9.0f, 0.0f, 0.0f}});
            std::size_t collapsed = 0;
            if (ri::scene::QuantizeNativeAnimationTimes(grid, 30.0, &collapsed) == 0U
                || collapsed == 0U
                || grid.tracks.empty()
                || grid.tracks.front().keys.size() != 1U
                || std::abs(grid.tracks.front().keys.front().translation.x - 9.0f) > 0.001f) {
                std::cerr << "Quantize did not collapse near-frame keys to the later sample.\n";
                return EXIT_FAILURE;
            }
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
        {
            ri::content::NativeAnimationDocument rev =
                ri::content::CreateNativeAnimationDocument("rev", "Rev", "humanoid.ri_rig.json");
            rev.durationSeconds = 1.0;
            ri::scene::UpsertNativeAnimationKey(
                rev, "left_hand", 0.25, ri::scene::Transform{.position = {0.1f, 0.0f, 0.0f}});
            ri::scene::UpsertNativeAnimationEvent(rev, 0.25, "mid");
            if (!ri::scene::ReverseNativeAnimation(rev)
                || rev.tracks.empty()
                || std::abs(rev.tracks.front().keys.front().timeSeconds - 0.75) > 0.01
                || rev.events.empty()
                || std::abs(rev.events.front().timeSeconds - 0.75) > 0.01) {
                std::cerr << "Reverse animation failed.\n";
                return EXIT_FAILURE;
            }
        }
        {
            ri::content::NativeAnimationDocument dense =
                ri::content::CreateNativeAnimationDocument("dense", "Dense", "humanoid.ri_rig.json");
            dense.durationSeconds = 1.0;
            ri::scene::UpsertNativeAnimationKey(
                dense, "left_hand", 0.0, ri::scene::Transform{.position = {0.0f, 0.0f, 0.0f}});
            ri::scene::UpsertNativeAnimationKey(
                dense, "left_hand", 1.0, ri::scene::Transform{.position = {1.0f, 0.0f, 0.0f}});
            const std::size_t written = ri::scene::DensifyNativeAnimationKeys(dense, 10.0);
            if (written == 0U || dense.tracks.empty() || dense.tracks.front().keys.size() < 11U) {
                std::cerr << "Densify animation keys failed.\n";
                return EXIT_FAILURE;
            }
            const auto& mid = dense.tracks.front().keys[5];
            if (std::abs(mid.timeSeconds - 0.5) > 0.02
                || std::abs(mid.translation.x - 0.5f) > 0.05f) {
                std::cerr << "Densify mid-key was not lerped.\n";
                return EXIT_FAILURE;
            }
            const std::size_t thinned = ri::scene::DecimateNativeAnimationKeys(dense);
            if (thinned == 0U || dense.tracks.front().keys.size() != 2U) {
                std::cerr << "Decimate animation keys failed to collapse a linear track.\n";
                return EXIT_FAILURE;
            }
        }
        {
            ri::content::NativeAnimationDocument curve =
                ri::content::CreateNativeAnimationDocument("curve", "Curve", "humanoid.ri_rig.json");
            curve.durationSeconds = 1.0;
            ri::scene::UpsertNativeAnimationKey(
                curve, "left_hand", 0.0, ri::scene::Transform{.position = {0.0f, 0.0f, 0.0f}});
            ri::scene::UpsertNativeAnimationKey(
                curve, "left_hand", 0.5, ri::scene::Transform{.position = {0.0f, 1.0f, 0.0f}});
            ri::scene::UpsertNativeAnimationKey(
                curve, "left_hand", 1.0, ri::scene::Transform{.position = {1.0f, 0.0f, 0.0f}});
            if (ri::scene::DecimateNativeAnimationKeys(curve) != 0U
                || curve.tracks.front().keys.size() != 3U) {
                std::cerr << "Decimate removed a non-linear key.\n";
                return EXIT_FAILURE;
            }
            if (ri::scene::SmoothNativeAnimationKeys(curve, 0.5f) == 0U
                || std::abs(curve.tracks.front().keys[1].translation.y - 0.5f) > 0.05f) {
                std::cerr << "Smooth animation keys did not ease the mid spike.\n";
                return EXIT_FAILURE;
            }
            if (ri::scene::ScaleNativeAnimationTransforms(curve, 2.0f) == 0U
                || std::abs(curve.tracks.front().keys[1].translation.y - 1.0f) > 0.05f) {
                std::cerr << "Scale animation transforms failed.\n";
                return EXIT_FAILURE;
            }
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

    const ri::scene::RigDefinition humanoid = ri::scene::CreateHumanoidRigDefinition("smoke_walk");
    ri::content::NativeAnimationDocument walk =
        ri::content::CreateNativeAnimationDocument("smoke_walk", "Smoke Walk", "rigs/smoke.ri_rig.json");
    ri::scene::AuthorHumanoidWalkClip(walk, humanoid, {.intensity = 1.0f});
    const auto walkReport = ri::content::ValidateNativeAnimationDocument(walk);
    if (!walkReport.valid || walk.tracks.empty() || walk.events.size() < 2U || !walk.looping) {
        std::cerr << "AuthorHumanoidWalkClip did not produce a valid looping walk.\n";
        return EXIT_FAILURE;
    }
    ri::content::NativeAnimationDocument idle =
        ri::content::CreateNativeAnimationDocument("smoke_idle", "Smoke Idle", "rigs/smoke.ri_rig.json");
    ri::scene::AuthorHumanoidIdleClip(idle, humanoid, {.intensity = 1.0f});
    const auto idleReport = ri::content::ValidateNativeAnimationDocument(idle);
    if (!idleReport.valid || idle.tracks.empty() || !idle.looping) {
        std::cerr << "AuthorHumanoidIdleClip did not produce a valid looping idle.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
