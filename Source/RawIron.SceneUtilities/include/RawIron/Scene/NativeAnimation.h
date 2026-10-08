#pragma once

#include "RawIron/Content/NativeAnimationDocument.h"
#include "RawIron/Scene/Animation.h"
#include "RawIron/Scene/RigAuthoring.h"
#include "RawIron/Scene/Scene.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ri::scene {

/// Seeds rest-pose keys at time 0 for every deform bone.
void SeedNativeAnimationFromRig(ri::content::NativeAnimationDocument& document, const RigDefinition& rig);

[[nodiscard]] AnimationClip BindNativeAnimationClip(
    const ri::content::NativeAnimationDocument& document,
    const Scene& scene,
    std::span<const int> boneNodes);

[[nodiscard]] int FindNativeAnimationBoneNode(
    const Scene& scene,
    std::span<const int> boneNodes,
    std::string_view boneName);

void UpsertNativeAnimationKey(
    ri::content::NativeAnimationDocument& document,
    std::string_view boneName,
    double timeSeconds,
    const Transform& transform);

/// Drops the key on `boneName` at `timeSeconds` (1/120s snap). Empty tracks are removed.
[[nodiscard]] bool RemoveNativeAnimationKey(
    ri::content::NativeAnimationDocument& document,
    std::string_view boneName,
    double timeSeconds);

/// Finds the nearest key time strictly before (`previous`) or after (`!previous`) `timeSeconds`.
/// When `boneName` is empty, searches every track. Returns nullopt when none exists.
[[nodiscard]] std::optional<double> FindNearestNativeAnimationKeyTime(
    const ri::content::NativeAnimationDocument& document,
    double timeSeconds,
    bool previous,
    std::string_view boneName = {});

/// Closest key on either side of `timeSeconds`. Prefers the earlier key on a tie.
[[nodiscard]] std::optional<double> FindClosestNativeAnimationKeyTime(
    const ri::content::NativeAnimationDocument& document,
    double timeSeconds,
    std::string_view boneName = {});

/// Writes rest-pose keys from `rig` at `timeSeconds`. Empty `boneName` keys every deform bone.
[[nodiscard]] std::size_t InsertNativeAnimationRestKeys(
    ri::content::NativeAnimationDocument& document,
    const RigDefinition& rig,
    double timeSeconds,
    std::string_view boneName = {});

/// Upserts a key at `timeSeconds` using the previous key's transform on `boneName` (hold plateau).
/// Returns false when no earlier key exists.
[[nodiscard]] bool HoldNativeAnimationKey(
    ri::content::NativeAnimationDocument& document,
    std::string_view boneName,
    double timeSeconds);

/// Holds every track that has an earlier key at `timeSeconds`. Returns bones held.
[[nodiscard]] std::size_t HoldNativeAnimationPose(
    ri::content::NativeAnimationDocument& document,
    double timeSeconds);

/// Upserts an interpolated key on `boneName` at `timeSeconds` between the surrounding keys.
/// Returns false when the playhead is not strictly between two keys.
[[nodiscard]] bool BreakdownNativeAnimationKey(
    ri::content::NativeAnimationDocument& document,
    std::string_view boneName,
    double timeSeconds);

/// Breakdowns every track with surrounding keys at `timeSeconds`. Returns bones stamped.
[[nodiscard]] std::size_t BreakdownNativeAnimationPose(
    ri::content::NativeAnimationDocument& document,
    double timeSeconds);

/// Copies `sourceBoneName` keys onto its left/right partner with X-mirrored transforms.
[[nodiscard]] std::size_t MirrorNativeAnimationTrackAcrossX(
    ri::content::NativeAnimationDocument& document,
    std::string_view sourceBoneName);

/// Scales every key/event time and the clip duration by `factor` (> 0).
[[nodiscard]] bool ScaleNativeAnimationTime(
    ri::content::NativeAnimationDocument& document,
    double factor);

/// Scales every key translation and rotationDegrees by `factor` (> 0). Scale channels stay.
[[nodiscard]] std::size_t ScaleNativeAnimationTransforms(
    ri::content::NativeAnimationDocument& document,
    float factor);

/// Adds `deltaSeconds` to every key/event time (clamped). Duration grows when delta is positive.
/// Keys that land on the same snap frame are collapsed (later sample wins).
[[nodiscard]] bool OffsetNativeAnimationTimes(
    ri::content::NativeAnimationDocument& document,
    double deltaSeconds);

/// Adds `deltaSeconds` only to `boneName` keys (clamped). Duration grows when needed.
/// Keys that land on the same snap frame are collapsed (later sample wins).
[[nodiscard]] bool OffsetNativeAnimationBoneTimes(
    ri::content::NativeAnimationDocument& document,
    std::string_view boneName,
    double deltaSeconds);

/// Shifts keys/events so the earliest key (or event) lands at time 0. Duration shrinks by the same delta.
[[nodiscard]] bool AlignNativeAnimationStart(ri::content::NativeAnimationDocument& document);

/// Sets duration to the latest key/event time (at least one snap unit).
[[nodiscard]] bool FitNativeAnimationDuration(ri::content::NativeAnimationDocument& document);

/// Replaces `destinationBoneName` keys with a copy of `sourceBoneName` keys. Returns copied key count.
[[nodiscard]] std::size_t CopyNativeAnimationTrack(
    ri::content::NativeAnimationDocument& document,
    std::string_view sourceBoneName,
    std::string_view destinationBoneName);

/// Replaces `destinationBoneName` keys with `keys` without touching any other track.
[[nodiscard]] std::size_t ReplaceNativeAnimationTrackKeys(
    ri::content::NativeAnimationDocument& document,
    std::string_view destinationBoneName,
    std::span<const ri::content::NativeAnimationKeyframe> keys);

/// Writes a key for every bound bone node at `timeSeconds`.
void CaptureNativeAnimationPose(
    ri::content::NativeAnimationDocument& document,
    const Scene& scene,
    std::span<const int> boneNodes,
    double timeSeconds);

/// Sets clip length without deleting keys. Keys past the new end stay until trim.
[[nodiscard]] bool SetNativeAnimationDuration(
    ri::content::NativeAnimationDocument& document,
    double durationSeconds);

struct NativeAnimationTrimResult {
    bool valid = false;
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    std::size_t removedKeys = 0;
    std::size_t removedEvents = 0;
    std::string summary{};
};

/// Drops keys outside `[startSeconds, endSeconds]`. When `shiftToZero`, remaining times
/// subtract `startSeconds` and duration becomes the window length.
[[nodiscard]] NativeAnimationTrimResult TrimNativeAnimation(
    ri::content::NativeAnimationDocument& document,
    double startSeconds,
    double endSeconds,
    bool shiftToZero = true);

/// Inserts or replaces a named marker at `timeSeconds` (same name within 1/120s snaps).
[[nodiscard]] bool UpsertNativeAnimationEvent(
    ri::content::NativeAnimationDocument& document,
    double timeSeconds,
    std::string_view name);

[[nodiscard]] bool RemoveNativeAnimationEvent(
    ri::content::NativeAnimationDocument& document,
    std::size_t index);

/// Renames the event at `index`. Empty names are rejected.
[[nodiscard]] bool RenameNativeAnimationEvent(
    ri::content::NativeAnimationDocument& document,
    std::size_t index,
    std::string_view name);

/// Moves the event at `index` to `timeSeconds` (clamped). Extends duration when needed.
[[nodiscard]] bool SetNativeAnimationEventTime(
    ri::content::NativeAnimationDocument& document,
    std::size_t index,
    double timeSeconds);

/// Copies the event at `index` onto `timeSeconds` (same name upsert). Extends duration when needed.
[[nodiscard]] bool DuplicateNativeAnimationEventAt(
    ri::content::NativeAnimationDocument& document,
    std::size_t index,
    double timeSeconds);

/// Clears every key on `boneName` (removes the track). Returns removed key count.
[[nodiscard]] std::size_t ClearNativeAnimationTrack(
    ri::content::NativeAnimationDocument& document,
    std::string_view boneName);

/// Clears keys on every track. Events and duration are preserved. Returns removed key count.
[[nodiscard]] std::size_t ClearNativeAnimationKeys(ri::content::NativeAnimationDocument& document);

/// Clears every event marker. Returns removed event count.
[[nodiscard]] std::size_t ClearNativeAnimationEvents(ri::content::NativeAnimationDocument& document);

/// Drops consecutive keys whose transforms match within epsilon. Returns removed key count.
[[nodiscard]] std::size_t DeduplicateNativeAnimationKeys(
    ri::content::NativeAnimationDocument& document,
    float positionEpsilon = 0.0001f,
    float rotationEpsilonDegrees = 0.05f,
    float scaleEpsilon = 0.0001f);

/// Snaps every key/event time to the nearest `framesPerSecond` grid.
/// Returns moved+collapsed stamp count. When `collapsedOut` is set, writes how many keys were
/// dropped because they landed on the same frame.
[[nodiscard]] std::size_t QuantizeNativeAnimationTimes(
    ri::content::NativeAnimationDocument& document,
    double framesPerSecond = 30.0,
    std::size_t* collapsedOut = nullptr);

/// Mirrors every key/event time around the clip midpoint (`duration - t`). Duration stays.
[[nodiscard]] bool ReverseNativeAnimation(ri::content::NativeAnimationDocument& document);

/// Inserts interpolated keys on every track at `framesPerSecond` between the first and last key.
/// Returns newly written key count (existing snaps count as updates).
[[nodiscard]] std::size_t DensifyNativeAnimationKeys(
    ri::content::NativeAnimationDocument& document,
    double framesPerSecond = 30.0);

/// Drops interior keys that already lie on the linear lerp between their neighbors (within epsilon).
/// Endpoints of each track are kept. Returns removed key count.
[[nodiscard]] std::size_t DecimateNativeAnimationKeys(
    ri::content::NativeAnimationDocument& document,
    float positionEpsilon = 0.0005f,
    float rotationEpsilonDegrees = 0.1f,
    float scaleEpsilon = 0.0005f);

/// One temporal laplacian pass on interior keys (blend toward neighbor lerp). Returns changed keys.
[[nodiscard]] std::size_t SmoothNativeAnimationKeys(
    ri::content::NativeAnimationDocument& document,
    float blend = 0.5f);

[[nodiscard]] bool RenameNativeAnimationBone(
    ri::content::NativeAnimationDocument& document,
    std::string_view oldName,
    std::string_view newName);

[[nodiscard]] bool RemoveNativeAnimationBone(
    ri::content::NativeAnimationDocument& document,
    std::string_view boneName);

struct NativeAnimationBindReport {
    std::size_t trackCount = 0;
    std::size_t boundTrackCount = 0;
    std::size_t missingBoneCount = 0;
    std::vector<std::string> missingBones{};
    std::string summary{};
};

/// Counts clip tracks that resolve to preview bone names. Unmatched names are diagnostics only.
[[nodiscard]] NativeAnimationBindReport DiagnoseNativeAnimationBind(
    const ri::content::NativeAnimationDocument& document,
    const Scene& scene,
    std::span<const int> boneNodes);

/// Removes tracks whose bone names do not resolve in `boneNodes`. Returns removed track count.
[[nodiscard]] std::size_t StripNativeAnimationMissingTracks(
    ri::content::NativeAnimationDocument& document,
    const Scene& scene,
    std::span<const int> boneNodes);

/// Prefers a bone named `root`, otherwise the hierarchy root among `boneNodes`.
[[nodiscard]] int FindNativeAnimationRootMotionNode(
    const Scene& scene,
    std::span<const int> boneNodes);

/// Keeps clip rotation/scale on the root bone and restores rest translation for in-place playback.
void HoldNativeAnimationRootInPlace(
    Scene& scene,
    int rootNode,
    const Transform& restLocal);

/// Shared humanoid clip recipes used by `ri_tool --forge-character-create` and re-author verbs.
struct HumanoidMotionAuthorOptions {
    /// Scales keyed rotation/translation deltas (1 = stock PSX scout).
    float intensity = 1.0f;
    bool looping = true;
};

/// Breathing + weight-shift idle (1s loop). Seeds from `rig` rest, then stamps deltas.
void AuthorHumanoidIdleClip(
    ri::content::NativeAnimationDocument& document,
    const RigDefinition& rig,
    const HumanoidMotionAuthorOptions& options = {});

/// Choppy 4-pose PSX walk (1s loop) with counter-rotated torso and arm swing.
void AuthorHumanoidWalkClip(
    ri::content::NativeAnimationDocument& document,
    const RigDefinition& rig,
    const HumanoidMotionAuthorOptions& options = {});

} // namespace ri::scene
