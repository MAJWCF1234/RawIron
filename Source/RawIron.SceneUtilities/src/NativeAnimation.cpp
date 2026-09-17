#include "RawIron/Scene/NativeAnimation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace ri::scene {
namespace {

constexpr double kKeySnap = 1.0 / 120.0;
constexpr double kMaxDurationSeconds = 600.0;

[[nodiscard]] ri::content::NativeAnimationKeyframe ToKey(const double timeSeconds, const Transform& transform) {
    return ri::content::NativeAnimationKeyframe{
        .timeSeconds = timeSeconds,
        .translation = {transform.position.x, transform.position.y, transform.position.z},
        .rotationDegrees = {
            transform.rotationDegrees.x,
            transform.rotationDegrees.y,
            transform.rotationDegrees.z,
        },
        .scale = {transform.scale.x, transform.scale.y, transform.scale.z},
    };
}

[[nodiscard]] Transform FromKey(const ri::content::NativeAnimationKeyframe& key) {
    return Transform{
        .position = {key.translation.x, key.translation.y, key.translation.z},
        .rotationDegrees = {key.rotationDegrees.x, key.rotationDegrees.y, key.rotationDegrees.z},
        .scale = {key.scale.x, key.scale.y, key.scale.z},
    };
}

[[nodiscard]] std::string TrimEventName(std::string_view value) {
    std::size_t begin = 0;
    while (begin < value.size()
           && (value[begin] == ' ' || value[begin] == '\t' || value[begin] == '\n' || value[begin] == '\r')) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin
           && (value[end - 1U] == ' ' || value[end - 1U] == '\t' || value[end - 1U] == '\n'
               || value[end - 1U] == '\r')) {
        --end;
    }
    std::string name(value.substr(begin, end - begin));
    if (name.size() > static_cast<std::size_t>(ri::content::NativeAnimationDocument::kMaxEventName)) {
        name.resize(static_cast<std::size_t>(ri::content::NativeAnimationDocument::kMaxEventName));
    }
    return name;
}

void SortNativeAnimationEvents(ri::content::NativeAnimationDocument& document) {
    std::stable_sort(
        document.events.begin(),
        document.events.end(),
        [](const ri::content::NativeAnimationEvent& lhs, const ri::content::NativeAnimationEvent& rhs) {
            if (std::abs(lhs.timeSeconds - rhs.timeSeconds) > kKeySnap) {
                return lhs.timeSeconds < rhs.timeSeconds;
            }
            return lhs.name < rhs.name;
        });
}

} // namespace

void SeedNativeAnimationFromRig(ri::content::NativeAnimationDocument& document, const RigDefinition& rig) {
    document.tracks.clear();
    document.tracks.reserve(rig.bones.size());
    for (const RigBone& bone : rig.bones) {
        if (bone.name.empty()) {
            continue;
        }
        document.tracks.push_back(ri::content::NativeAnimationTrack{
            .boneName = bone.name,
            .keys = {ToKey(0.0, bone.restLocal)},
        });
    }
    if (document.durationSeconds <= 0.0) {
        document.durationSeconds = 1.0;
    }
}

int FindNativeAnimationBoneNode(
    const Scene& scene,
    const std::span<const int> boneNodes,
    const std::string_view boneName) {
    if (boneName.empty()) {
        return kInvalidHandle;
    }
    for (const int boneNode : boneNodes) {
        if (boneNode == kInvalidHandle || boneNode < 0
            || static_cast<std::size_t>(boneNode) >= scene.NodeCount()) {
            continue;
        }
        if (scene.GetNode(boneNode).name == boneName) {
            return boneNode;
        }
    }
    return kInvalidHandle;
}

AnimationClip BindNativeAnimationClip(
    const ri::content::NativeAnimationDocument& document,
    const Scene& scene,
    const std::span<const int> boneNodes) {
    AnimationClip clip{};
    clip.name = document.displayName.empty() ? document.id : document.displayName;
    clip.durationSeconds = document.durationSeconds;
    clip.looping = document.looping;
    for (const ri::content::NativeAnimationTrack& track : document.tracks) {
        const int nodeHandle = FindNativeAnimationBoneNode(scene, boneNodes, track.boneName);
        if (nodeHandle == kInvalidHandle) {
            continue;
        }
        std::vector<TransformKeyframe>& keys = clip.nodeTracks[nodeHandle];
        keys.reserve(track.keys.size());
        for (const ri::content::NativeAnimationKeyframe& key : track.keys) {
            keys.push_back(TransformKeyframe{
                .timeSeconds = key.timeSeconds,
                .transform = FromKey(key),
            });
        }
    }
    return clip;
}

void UpsertNativeAnimationKey(
    ri::content::NativeAnimationDocument& document,
    const std::string_view boneName,
    const double timeSeconds,
    const Transform& transform) {
    if (boneName.empty() || !std::isfinite(timeSeconds) || timeSeconds < 0.0) {
        return;
    }
    ri::content::NativeAnimationTrack* track = nullptr;
    for (ri::content::NativeAnimationTrack& candidate : document.tracks) {
        if (candidate.boneName == boneName) {
            track = &candidate;
            break;
        }
    }
    if (track == nullptr) {
        document.tracks.push_back(ri::content::NativeAnimationTrack{.boneName = std::string(boneName)});
        track = &document.tracks.back();
    }
    const ri::content::NativeAnimationKeyframe next = ToKey(timeSeconds, transform);
    for (ri::content::NativeAnimationKeyframe& key : track->keys) {
        if (std::abs(key.timeSeconds - timeSeconds) <= kKeySnap) {
            key = next;
            return;
        }
    }
    track->keys.push_back(next);
    std::stable_sort(
        track->keys.begin(),
        track->keys.end(),
        [](const ri::content::NativeAnimationKeyframe& lhs, const ri::content::NativeAnimationKeyframe& rhs) {
            return lhs.timeSeconds < rhs.timeSeconds;
        });
    if (timeSeconds > document.durationSeconds) {
        document.durationSeconds = timeSeconds;
    }
}

bool RemoveNativeAnimationKey(
    ri::content::NativeAnimationDocument& document,
    const std::string_view boneName,
    const double timeSeconds) {
    if (boneName.empty() || !std::isfinite(timeSeconds) || timeSeconds < 0.0) {
        return false;
    }
    for (auto track = document.tracks.begin(); track != document.tracks.end(); ++track) {
        if (track->boneName != boneName) {
            continue;
        }
        const auto key = std::find_if(
            track->keys.begin(),
            track->keys.end(),
            [timeSeconds](const ri::content::NativeAnimationKeyframe& candidate) {
                return std::abs(candidate.timeSeconds - timeSeconds) <= kKeySnap;
            });
        if (key == track->keys.end()) {
            return false;
        }
        track->keys.erase(key);
        if (track->keys.empty()) {
            document.tracks.erase(track);
        }
        return true;
    }
    return false;
}

std::optional<double> FindNearestNativeAnimationKeyTime(
    const ri::content::NativeAnimationDocument& document,
    const double timeSeconds,
    const bool previous,
    const std::string_view boneName) {
    if (!std::isfinite(timeSeconds)) {
        return std::nullopt;
    }
    std::optional<double> best{};
    for (const ri::content::NativeAnimationTrack& track : document.tracks) {
        if (!boneName.empty() && track.boneName != boneName) {
            continue;
        }
        for (const ri::content::NativeAnimationKeyframe& key : track.keys) {
            if (!std::isfinite(key.timeSeconds)) {
                continue;
            }
            if (previous) {
                if (key.timeSeconds >= timeSeconds - kKeySnap) {
                    continue;
                }
                if (!best.has_value() || key.timeSeconds > *best) {
                    best = key.timeSeconds;
                }
            } else {
                if (key.timeSeconds <= timeSeconds + kKeySnap) {
                    continue;
                }
                if (!best.has_value() || key.timeSeconds < *best) {
                    best = key.timeSeconds;
                }
            }
        }
    }
    return best;
}

std::optional<double> FindClosestNativeAnimationKeyTime(
    const ri::content::NativeAnimationDocument& document,
    const double timeSeconds,
    const std::string_view boneName) {
    if (!std::isfinite(timeSeconds)) {
        return std::nullopt;
    }
    std::optional<double> best{};
    double bestDistance = 0.0;
    for (const ri::content::NativeAnimationTrack& track : document.tracks) {
        if (!boneName.empty() && track.boneName != boneName) {
            continue;
        }
        for (const ri::content::NativeAnimationKeyframe& key : track.keys) {
            if (!std::isfinite(key.timeSeconds)) {
                continue;
            }
            const double distance = std::abs(key.timeSeconds - timeSeconds);
            if (!best.has_value()
                || distance < bestDistance - 1.0e-9
                || (std::abs(distance - bestDistance) <= 1.0e-9 && key.timeSeconds < *best)) {
                best = key.timeSeconds;
                bestDistance = distance;
            }
        }
    }
    return best;
}

std::size_t InsertNativeAnimationRestKeys(
    ri::content::NativeAnimationDocument& document,
    const RigDefinition& rig,
    const double timeSeconds,
    const std::string_view boneName) {
    if (!std::isfinite(timeSeconds) || timeSeconds < 0.0) {
        return 0;
    }
    std::size_t changed = 0;
    for (const RigBone& bone : rig.bones) {
        if (bone.name.empty() || !bone.deform) {
            continue;
        }
        if (!boneName.empty() && bone.name != boneName) {
            continue;
        }
        UpsertNativeAnimationKey(document, bone.name, timeSeconds, bone.restLocal);
        ++changed;
    }
    return changed;
}

std::size_t MirrorNativeAnimationTrackAcrossX(
    ri::content::NativeAnimationDocument& document,
    const std::string_view sourceBoneName) {
    if (sourceBoneName.empty()) {
        return 0;
    }
    const std::optional<std::string> partnerName = MirrorPartnerBoneName(sourceBoneName);
    if (!partnerName.has_value()) {
        return 0;
    }
    const ri::content::NativeAnimationTrack* source = nullptr;
    for (const ri::content::NativeAnimationTrack& track : document.tracks) {
        if (track.boneName == sourceBoneName) {
            source = &track;
            break;
        }
    }
    if (source == nullptr || source->keys.empty()) {
        return 0;
    }
    // Copy keys first — Upsert may reallocate tracks.
    const std::vector<ri::content::NativeAnimationKeyframe> keys = source->keys;
    std::size_t changed = 0;
    for (const ri::content::NativeAnimationKeyframe& key : keys) {
        UpsertNativeAnimationKey(
            document, *partnerName, key.timeSeconds, MirrorTransformAcrossX(FromKey(key)));
        ++changed;
    }
    return changed;
}

bool ScaleNativeAnimationTime(
    ri::content::NativeAnimationDocument& document,
    const double factor) {
    if (!std::isfinite(factor) || factor <= 0.0) {
        return false;
    }
    if (std::abs(factor - 1.0) <= 1.0e-12) {
        return true;
    }
    for (ri::content::NativeAnimationTrack& track : document.tracks) {
        for (ri::content::NativeAnimationKeyframe& key : track.keys) {
            key.timeSeconds = std::clamp(key.timeSeconds * factor, 0.0, kMaxDurationSeconds);
        }
        std::sort(
            track.keys.begin(),
            track.keys.end(),
            [](const ri::content::NativeAnimationKeyframe& lhs,
               const ri::content::NativeAnimationKeyframe& rhs) {
                return lhs.timeSeconds < rhs.timeSeconds;
            });
    }
    for (ri::content::NativeAnimationEvent& event : document.events) {
        event.timeSeconds = std::clamp(event.timeSeconds * factor, 0.0, kMaxDurationSeconds);
    }
    SortNativeAnimationEvents(document);
    document.durationSeconds =
        std::clamp(document.durationSeconds * factor, kKeySnap, kMaxDurationSeconds);
    return true;
}

bool OffsetNativeAnimationTimes(
    ri::content::NativeAnimationDocument& document,
    const double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || std::abs(deltaSeconds) <= 1.0e-12) {
        return false;
    }
    for (ri::content::NativeAnimationTrack& track : document.tracks) {
        for (ri::content::NativeAnimationKeyframe& key : track.keys) {
            key.timeSeconds = std::clamp(key.timeSeconds + deltaSeconds, 0.0, kMaxDurationSeconds);
        }
        std::sort(
            track.keys.begin(),
            track.keys.end(),
            [](const ri::content::NativeAnimationKeyframe& lhs,
               const ri::content::NativeAnimationKeyframe& rhs) {
                return lhs.timeSeconds < rhs.timeSeconds;
            });
    }
    for (ri::content::NativeAnimationEvent& event : document.events) {
        event.timeSeconds = std::clamp(event.timeSeconds + deltaSeconds, 0.0, kMaxDurationSeconds);
    }
    SortNativeAnimationEvents(document);
    if (deltaSeconds > 0.0) {
        document.durationSeconds =
            std::clamp(document.durationSeconds + deltaSeconds, kKeySnap, kMaxDurationSeconds);
    }
    return true;
}

std::size_t CopyNativeAnimationTrack(
    ri::content::NativeAnimationDocument& document,
    const std::string_view sourceBoneName,
    const std::string_view destinationBoneName) {
    if (sourceBoneName.empty() || destinationBoneName.empty()
        || sourceBoneName == destinationBoneName) {
        return 0;
    }
    const ri::content::NativeAnimationTrack* source = nullptr;
    for (const ri::content::NativeAnimationTrack& track : document.tracks) {
        if (track.boneName == sourceBoneName) {
            source = &track;
            break;
        }
    }
    if (source == nullptr || source->keys.empty()) {
        return 0;
    }
    const std::vector<ri::content::NativeAnimationKeyframe> keys = source->keys;
    // Drop the destination track first so Upsert recreates a clean copy.
    for (auto track = document.tracks.begin(); track != document.tracks.end(); ++track) {
        if (track->boneName == destinationBoneName) {
            document.tracks.erase(track);
            break;
        }
    }
    std::size_t changed = 0;
    for (const ri::content::NativeAnimationKeyframe& key : keys) {
        UpsertNativeAnimationKey(document, destinationBoneName, key.timeSeconds, FromKey(key));
        ++changed;
    }
    return changed;
}

[[nodiscard]] std::optional<double> FindEarliestNativeAnimationTime(
    const ri::content::NativeAnimationDocument& document) {
    std::optional<double> earliest{};
    for (const ri::content::NativeAnimationTrack& track : document.tracks) {
        for (const ri::content::NativeAnimationKeyframe& key : track.keys) {
            if (!std::isfinite(key.timeSeconds)) {
                continue;
            }
            if (!earliest.has_value() || key.timeSeconds < *earliest) {
                earliest = key.timeSeconds;
            }
        }
    }
    for (const ri::content::NativeAnimationEvent& event : document.events) {
        if (!std::isfinite(event.timeSeconds)) {
            continue;
        }
        if (!earliest.has_value() || event.timeSeconds < *earliest) {
            earliest = event.timeSeconds;
        }
    }
    return earliest;
}

[[nodiscard]] std::optional<double> FindLatestNativeAnimationTime(
    const ri::content::NativeAnimationDocument& document) {
    std::optional<double> latest{};
    for (const ri::content::NativeAnimationTrack& track : document.tracks) {
        for (const ri::content::NativeAnimationKeyframe& key : track.keys) {
            if (!std::isfinite(key.timeSeconds)) {
                continue;
            }
            if (!latest.has_value() || key.timeSeconds > *latest) {
                latest = key.timeSeconds;
            }
        }
    }
    for (const ri::content::NativeAnimationEvent& event : document.events) {
        if (!std::isfinite(event.timeSeconds)) {
            continue;
        }
        if (!latest.has_value() || event.timeSeconds > *latest) {
            latest = event.timeSeconds;
        }
    }
    return latest;
}

bool AlignNativeAnimationStart(ri::content::NativeAnimationDocument& document) {
    const std::optional<double> earliest = FindEarliestNativeAnimationTime(document);
    if (!earliest.has_value()) {
        return false;
    }
    if (*earliest <= kKeySnap) {
        return true;
    }
    const double delta = *earliest;
    for (ri::content::NativeAnimationTrack& track : document.tracks) {
        for (ri::content::NativeAnimationKeyframe& key : track.keys) {
            key.timeSeconds = std::max(0.0, key.timeSeconds - delta);
        }
    }
    for (ri::content::NativeAnimationEvent& event : document.events) {
        event.timeSeconds = std::max(0.0, event.timeSeconds - delta);
    }
    SortNativeAnimationEvents(document);
    document.durationSeconds =
        std::clamp(std::max(document.durationSeconds - delta, kKeySnap), kKeySnap, kMaxDurationSeconds);
    return true;
}

bool FitNativeAnimationDuration(ri::content::NativeAnimationDocument& document) {
    const std::optional<double> latest = FindLatestNativeAnimationTime(document);
    if (!latest.has_value()) {
        return false;
    }
    document.durationSeconds = std::clamp(std::max(*latest, kKeySnap), kKeySnap, kMaxDurationSeconds);
    return true;
}

void CaptureNativeAnimationPose(
    ri::content::NativeAnimationDocument& document,
    const Scene& scene,
    const std::span<const int> boneNodes,
    const double timeSeconds) {
    for (const int boneNode : boneNodes) {
        if (boneNode == kInvalidHandle || boneNode < 0
            || static_cast<std::size_t>(boneNode) >= scene.NodeCount()) {
            continue;
        }
        const Node& node = scene.GetNode(boneNode);
        if (node.name.empty()) {
            continue;
        }
        UpsertNativeAnimationKey(document, node.name, timeSeconds, node.localTransform);
    }
}

bool SetNativeAnimationDuration(
    ri::content::NativeAnimationDocument& document,
    const double durationSeconds) {
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0 || durationSeconds > kMaxDurationSeconds) {
        return false;
    }
    document.durationSeconds = durationSeconds;
    return true;
}

NativeAnimationTrimResult TrimNativeAnimation(
    ri::content::NativeAnimationDocument& document,
    const double startSeconds,
    const double endSeconds,
    const bool shiftToZero) {
    NativeAnimationTrimResult result{
        .startSeconds = startSeconds,
        .endSeconds = endSeconds,
    };
    if (!std::isfinite(startSeconds) || !std::isfinite(endSeconds) || startSeconds < 0.0
        || endSeconds <= startSeconds || endSeconds > kMaxDurationSeconds) {
        result.summary = "Trim range must be finite, start >= 0, and end in (start, 600].";
        return result;
    }
    const double lo = startSeconds - kKeySnap;
    const double hi = endSeconds + kKeySnap;
    for (ri::content::NativeAnimationTrack& track : document.tracks) {
        const auto firstRemoved = std::remove_if(
            track.keys.begin(),
            track.keys.end(),
            [lo, hi](const ri::content::NativeAnimationKeyframe& key) {
                return key.timeSeconds < lo || key.timeSeconds > hi;
            });
        result.removedKeys += static_cast<std::size_t>(std::distance(firstRemoved, track.keys.end()));
        track.keys.erase(firstRemoved, track.keys.end());
        if (shiftToZero) {
            for (ri::content::NativeAnimationKeyframe& key : track.keys) {
                key.timeSeconds = std::max(0.0, key.timeSeconds - startSeconds);
            }
        }
    }
    const auto firstRemovedEvent = std::remove_if(
        document.events.begin(),
        document.events.end(),
        [lo, hi](const ri::content::NativeAnimationEvent& event) {
            return event.timeSeconds < lo || event.timeSeconds > hi;
        });
    result.removedEvents += static_cast<std::size_t>(std::distance(firstRemovedEvent, document.events.end()));
    document.events.erase(firstRemovedEvent, document.events.end());
    if (shiftToZero) {
        for (ri::content::NativeAnimationEvent& event : document.events) {
            event.timeSeconds = std::max(0.0, event.timeSeconds - startSeconds);
        }
    }
    SortNativeAnimationEvents(document);
    document.durationSeconds = shiftToZero ? (endSeconds - startSeconds) : endSeconds;
    result.valid = true;
    std::ostringstream summary;
    summary << "Trimmed " << result.removedKeys << " key"
            << (result.removedKeys == 1U ? "" : "s");
    if (result.removedEvents > 0U) {
        summary << ", " << result.removedEvents << " event"
                << (result.removedEvents == 1U ? "" : "s");
    }
    summary << ". Duration " << document.durationSeconds << "s.";
    result.summary = summary.str();
    return result;
}

bool UpsertNativeAnimationEvent(
    ri::content::NativeAnimationDocument& document,
    const double timeSeconds,
    const std::string_view name) {
    const std::string trimmed = TrimEventName(name);
    if (trimmed.empty() || !std::isfinite(timeSeconds) || timeSeconds < 0.0) {
        return false;
    }
    for (ri::content::NativeAnimationEvent& event : document.events) {
        if (event.name == trimmed && std::abs(event.timeSeconds - timeSeconds) <= kKeySnap) {
            event.timeSeconds = timeSeconds;
            SortNativeAnimationEvents(document);
            return true;
        }
    }
    if (document.events.size() >= static_cast<std::size_t>(ri::content::NativeAnimationDocument::kMaxEvents)) {
        return false;
    }
    document.events.push_back(ri::content::NativeAnimationEvent{
        .timeSeconds = timeSeconds,
        .name = trimmed,
    });
    SortNativeAnimationEvents(document);
    return true;
}

bool RemoveNativeAnimationEvent(
    ri::content::NativeAnimationDocument& document,
    const std::size_t index) {
    if (index >= document.events.size()) {
        return false;
    }
    document.events.erase(document.events.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool RenameNativeAnimationEvent(
    ri::content::NativeAnimationDocument& document,
    const std::size_t index,
    const std::string_view name) {
    if (index >= document.events.size()) {
        return false;
    }
    const std::string trimmed = TrimEventName(name);
    if (trimmed.empty()) {
        return false;
    }
    document.events[index].name = trimmed;
    SortNativeAnimationEvents(document);
    return true;
}

bool SetNativeAnimationEventTime(
    ri::content::NativeAnimationDocument& document,
    const std::size_t index,
    const double timeSeconds) {
    if (index >= document.events.size() || !std::isfinite(timeSeconds) || timeSeconds < 0.0) {
        return false;
    }
    const double clamped = std::clamp(timeSeconds, 0.0, kMaxDurationSeconds);
    if (std::abs(document.events[index].timeSeconds - clamped) <= kKeySnap) {
        return true;
    }
    document.events[index].timeSeconds = clamped;
    if (clamped > document.durationSeconds) {
        document.durationSeconds = clamped;
    }
    SortNativeAnimationEvents(document);
    return true;
}

bool DuplicateNativeAnimationEventAt(
    ri::content::NativeAnimationDocument& document,
    const std::size_t index,
    const double timeSeconds) {
    if (index >= document.events.size()) {
        return false;
    }
    return UpsertNativeAnimationEvent(document, timeSeconds, document.events[index].name);
}

NativeAnimationBindReport DiagnoseNativeAnimationBind(
    const ri::content::NativeAnimationDocument& document,
    const Scene& scene,
    const std::span<const int> boneNodes) {
    NativeAnimationBindReport report{};
    report.trackCount = document.tracks.size();
    for (const ri::content::NativeAnimationTrack& track : document.tracks) {
        if (FindNativeAnimationBoneNode(scene, boneNodes, track.boneName) == kInvalidHandle) {
            ++report.missingBoneCount;
            if (report.missingBones.size() < 8U) {
                report.missingBones.push_back(track.boneName);
            }
            continue;
        }
        ++report.boundTrackCount;
    }
    std::ostringstream summary;
    if (report.trackCount == 0U) {
        summary << "Clip has no bone tracks.";
    } else if (report.missingBoneCount == 0U) {
        summary << report.boundTrackCount << " track"
                << (report.boundTrackCount == 1U ? "" : "s") << " bound.";
    } else {
        summary << report.boundTrackCount << " of " << report.trackCount << " tracks bound. Missing: ";
        for (std::size_t index = 0; index < report.missingBones.size(); ++index) {
            if (index > 0U) {
                summary << ", ";
            }
            summary << report.missingBones[index];
        }
        if (report.missingBoneCount > report.missingBones.size()) {
            summary << ", +" << (report.missingBoneCount - report.missingBones.size()) << " more";
        }
        summary << ".";
    }
    report.summary = summary.str();
    return report;
}

std::size_t StripNativeAnimationMissingTracks(
    ri::content::NativeAnimationDocument& document,
    const Scene& scene,
    const std::span<const int> boneNodes) {
    const std::size_t before = document.tracks.size();
    document.tracks.erase(
        std::remove_if(
            document.tracks.begin(),
            document.tracks.end(),
            [&](const ri::content::NativeAnimationTrack& track) {
                return FindNativeAnimationBoneNode(scene, boneNodes, track.boneName) == kInvalidHandle;
            }),
        document.tracks.end());
    return before - document.tracks.size();
}

int FindNativeAnimationRootMotionNode(
    const Scene& scene,
    const std::span<const int> boneNodes) {
    const int named = FindNativeAnimationBoneNode(scene, boneNodes, "root");
    if (named != kInvalidHandle) {
        return named;
    }
    std::unordered_set<int> members{};
    for (const int boneNode : boneNodes) {
        if (boneNode != kInvalidHandle && boneNode >= 0
            && static_cast<std::size_t>(boneNode) < scene.NodeCount()) {
            members.insert(boneNode);
        }
    }
    for (const int boneNode : boneNodes) {
        if (!members.contains(boneNode)) {
            continue;
        }
        if (!members.contains(scene.GetNode(boneNode).parent)) {
            return boneNode;
        }
    }
    return kInvalidHandle;
}

void HoldNativeAnimationRootInPlace(
    Scene& scene,
    const int rootNode,
    const Transform& restLocal) {
    if (rootNode == kInvalidHandle || rootNode < 0
        || static_cast<std::size_t>(rootNode) >= scene.NodeCount()) {
        return;
    }
    scene.GetNode(rootNode).localTransform.position = restLocal.position;
}

bool RenameNativeAnimationBone(
    ri::content::NativeAnimationDocument& document,
    const std::string_view oldName,
    const std::string_view newName) {
    if (oldName.empty() || newName.empty() || oldName == newName) {
        return false;
    }
    bool renamed = false;
    for (ri::content::NativeAnimationTrack& track : document.tracks) {
        if (track.boneName == oldName) {
            track.boneName = std::string(newName);
            renamed = true;
        }
    }
    return renamed;
}

bool RemoveNativeAnimationBone(
    ri::content::NativeAnimationDocument& document,
    const std::string_view boneName) {
    if (boneName.empty()) {
        return false;
    }
    const std::size_t before = document.tracks.size();
    document.tracks.erase(
        std::remove_if(
            document.tracks.begin(),
            document.tracks.end(),
            [&](const ri::content::NativeAnimationTrack& track) { return track.boneName == boneName; }),
        document.tracks.end());
    return document.tracks.size() != before;
}

std::size_t ClearNativeAnimationTrack(
    ri::content::NativeAnimationDocument& document,
    const std::string_view boneName) {
    if (boneName.empty()) {
        return 0;
    }
    for (auto track = document.tracks.begin(); track != document.tracks.end(); ++track) {
        if (track->boneName != boneName) {
            continue;
        }
        const std::size_t removed = track->keys.size();
        document.tracks.erase(track);
        return removed;
    }
    return 0;
}

std::size_t ClearNativeAnimationKeys(ri::content::NativeAnimationDocument& document) {
    std::size_t removed = 0;
    for (ri::content::NativeAnimationTrack& track : document.tracks) {
        removed += track.keys.size();
        track.keys.clear();
    }
    document.tracks.erase(
        std::remove_if(
            document.tracks.begin(),
            document.tracks.end(),
            [](const ri::content::NativeAnimationTrack& track) { return track.keys.empty(); }),
        document.tracks.end());
    return removed;
}

std::size_t ClearNativeAnimationEvents(ri::content::NativeAnimationDocument& document) {
    const std::size_t removed = document.events.size();
    document.events.clear();
    return removed;
}

static bool KeysNearlyEqual(
    const ri::content::NativeAnimationKeyframe& lhs,
    const ri::content::NativeAnimationKeyframe& rhs,
    const float positionEpsilon,
    const float rotationEpsilonDegrees,
    const float scaleEpsilon) {
    const auto near = [](const float a, const float b, const float epsilon) {
        return std::abs(a - b) <= epsilon;
    };
    return near(lhs.translation.x, rhs.translation.x, positionEpsilon)
        && near(lhs.translation.y, rhs.translation.y, positionEpsilon)
        && near(lhs.translation.z, rhs.translation.z, positionEpsilon)
        && near(lhs.rotationDegrees.x, rhs.rotationDegrees.x, rotationEpsilonDegrees)
        && near(lhs.rotationDegrees.y, rhs.rotationDegrees.y, rotationEpsilonDegrees)
        && near(lhs.rotationDegrees.z, rhs.rotationDegrees.z, rotationEpsilonDegrees)
        && near(lhs.scale.x, rhs.scale.x, scaleEpsilon)
        && near(lhs.scale.y, rhs.scale.y, scaleEpsilon)
        && near(lhs.scale.z, rhs.scale.z, scaleEpsilon);
}

std::size_t DeduplicateNativeAnimationKeys(
    ri::content::NativeAnimationDocument& document,
    const float positionEpsilon,
    const float rotationEpsilonDegrees,
    const float scaleEpsilon) {
    if (positionEpsilon < 0.0f || rotationEpsilonDegrees < 0.0f || scaleEpsilon < 0.0f) {
        return 0;
    }
    std::size_t removed = 0;
    for (ri::content::NativeAnimationTrack& track : document.tracks) {
        if (track.keys.size() < 2U) {
            continue;
        }
        std::vector<ri::content::NativeAnimationKeyframe> kept{};
        kept.reserve(track.keys.size());
        kept.push_back(track.keys.front());
        for (std::size_t index = 1; index < track.keys.size(); ++index) {
            if (KeysNearlyEqual(
                    kept.back(),
                    track.keys[index],
                    positionEpsilon,
                    rotationEpsilonDegrees,
                    scaleEpsilon)) {
                ++removed;
                continue;
            }
            kept.push_back(track.keys[index]);
        }
        track.keys = std::move(kept);
    }
    document.tracks.erase(
        std::remove_if(
            document.tracks.begin(),
            document.tracks.end(),
            [](const ri::content::NativeAnimationTrack& track) { return track.keys.empty(); }),
        document.tracks.end());
    return removed;
}

std::size_t QuantizeNativeAnimationTimes(
    ri::content::NativeAnimationDocument& document,
    const double framesPerSecond) {
    if (!std::isfinite(framesPerSecond) || framesPerSecond < 1.0 || framesPerSecond > 240.0) {
        return 0;
    }
    const double frame = 1.0 / framesPerSecond;
    const auto quantize = [frame](const double timeSeconds) {
        const double snapped = std::round(timeSeconds / frame) * frame;
        return std::clamp(snapped, 0.0, kMaxDurationSeconds);
    };
    std::size_t changed = 0;
    for (ri::content::NativeAnimationTrack& track : document.tracks) {
        for (ri::content::NativeAnimationKeyframe& key : track.keys) {
            if (!std::isfinite(key.timeSeconds)) {
                continue;
            }
            const double next = quantize(key.timeSeconds);
            if (std::abs(next - key.timeSeconds) > 1.0e-9) {
                key.timeSeconds = next;
                ++changed;
            }
        }
        std::sort(
            track.keys.begin(),
            track.keys.end(),
            [](const ri::content::NativeAnimationKeyframe& lhs,
               const ri::content::NativeAnimationKeyframe& rhs) {
                return lhs.timeSeconds < rhs.timeSeconds;
            });
        // Collapse keys that landed on the same frame (keep the later sample).
        if (track.keys.size() >= 2U) {
            std::vector<ri::content::NativeAnimationKeyframe> kept{};
            kept.reserve(track.keys.size());
            for (const ri::content::NativeAnimationKeyframe& key : track.keys) {
                if (!kept.empty() && std::abs(kept.back().timeSeconds - key.timeSeconds) <= kKeySnap) {
                    kept.back() = key;
                    ++changed;
                    continue;
                }
                kept.push_back(key);
            }
            track.keys = std::move(kept);
        }
    }
    for (ri::content::NativeAnimationEvent& event : document.events) {
        if (!std::isfinite(event.timeSeconds)) {
            continue;
        }
        const double next = quantize(event.timeSeconds);
        if (std::abs(next - event.timeSeconds) > 1.0e-9) {
            event.timeSeconds = next;
            ++changed;
        }
    }
    SortNativeAnimationEvents(document);
    document.durationSeconds = quantize(document.durationSeconds);
    if (document.durationSeconds < kKeySnap) {
        document.durationSeconds = kKeySnap;
    }
    return changed;
}

} // namespace ri::scene
