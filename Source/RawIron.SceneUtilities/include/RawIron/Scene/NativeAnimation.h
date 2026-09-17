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

/// Copies `sourceBoneName` keys onto its left/right partner with X-mirrored transforms.
[[nodiscard]] std::size_t MirrorNativeAnimationTrackAcrossX(
    ri::content::NativeAnimationDocument& document,
    std::string_view sourceBoneName);

/// Scales every key/event time and the clip duration by `factor` (> 0).
[[nodiscard]] bool ScaleNativeAnimationTime(
    ri::content::NativeAnimationDocument& document,
    double factor);

/// Adds `deltaSeconds` to every key/event time (clamped). Duration grows when delta is positive.
[[nodiscard]] bool OffsetNativeAnimationTimes(
    ri::content::NativeAnimationDocument& document,
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

/// Snaps every key/event time to the nearest `framesPerSecond` grid. Returns changed stamp count.
[[nodiscard]] std::size_t QuantizeNativeAnimationTimes(
    ri::content::NativeAnimationDocument& document,
    double framesPerSecond = 30.0);

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

} // namespace ri::scene
