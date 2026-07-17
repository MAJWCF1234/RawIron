#include "RawIron/Scene/Animation.h"

#include "RawIron/Math/Vec3.h"

#include <algorithm>
#include <cmath>

namespace ri::scene {

namespace {

[[nodiscard]] bool HasUsableDuration(const AnimationClip& clip) noexcept {
    return std::isfinite(clip.durationSeconds) && clip.durationSeconds > 0.0;
}

[[nodiscard]] bool IsFinite(const ri::math::Vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool IsFinite(const Transform& transform) noexcept {
    return IsFinite(transform.position) && IsFinite(transform.rotationDegrees) && IsFinite(transform.scale);
}

[[nodiscard]] float LerpFinite(float a, float b, float t) noexcept {
    return static_cast<float>(static_cast<double>(a) * (1.0 - static_cast<double>(t))
                              + static_cast<double>(b) * static_cast<double>(t));
}

[[nodiscard]] ri::math::Vec3 LerpFinite(const ri::math::Vec3& a,
                                       const ri::math::Vec3& b,
                                       float t) noexcept {
    return {LerpFinite(a.x, b.x, t), LerpFinite(a.y, b.y, t), LerpFinite(a.z, b.z, t)};
}

double ClampTimeToClip(const AnimationClip& clip, double timeSeconds, bool looping) {
    if (!HasUsableDuration(clip)) {
        return 0.0;
    }
    if (!std::isfinite(timeSeconds)) {
        return 0.0;
    }
    if (!looping) {
        return std::clamp(timeSeconds, 0.0, clip.durationSeconds);
    }

    const double wrapped = std::fmod(timeSeconds, clip.durationSeconds);
    return wrapped < 0.0 ? wrapped + clip.durationSeconds : wrapped;
}

Transform SampleTrackAtTime(const std::vector<TransformKeyframe>& track, double timeSeconds) {
    if (track.empty() || !std::isfinite(timeSeconds)) {
        return Transform{};
    }

    std::vector<TransformKeyframe> sorted;
    sorted.reserve(track.size());
    for (const TransformKeyframe& keyframe : track) {
        if (std::isfinite(keyframe.timeSeconds) && IsFinite(keyframe.transform)) {
            sorted.push_back(keyframe);
        }
    }
    if (sorted.empty()) {
        return Transform{};
    }

    std::stable_sort(sorted.begin(), sorted.end(), [](const TransformKeyframe& lhs, const TransformKeyframe& rhs) {
        return lhs.timeSeconds < rhs.timeSeconds;
    });

    // Duplicate timestamps are common after timeline edits. Keep the last authored value so the
    // result is deterministic and never divides by a zero-width segment.
    auto output = sorted.begin();
    for (auto input = sorted.begin(); input != sorted.end(); ++input) {
        if (output != sorted.begin() && (output - 1)->timeSeconds == input->timeSeconds) {
            *(output - 1) = *input;
        } else {
            *output++ = *input;
        }
    }
    sorted.erase(output, sorted.end());

    if (timeSeconds <= sorted.front().timeSeconds) {
        return sorted.front().transform;
    }
    if (timeSeconds >= sorted.back().timeSeconds) {
        return sorted.back().transform;
    }

    for (std::size_t index = 0; index + 1 < sorted.size(); ++index) {
        const TransformKeyframe& a = sorted[index];
        const TransformKeyframe& b = sorted[index + 1];
        if (timeSeconds < a.timeSeconds || timeSeconds > b.timeSeconds) {
            continue;
        }

        const double span = b.timeSeconds - a.timeSeconds;
        if (!(span > 0.0) || !std::isfinite(span)) {
            continue;
        }
        const float t = std::clamp(static_cast<float>((timeSeconds - a.timeSeconds) / span), 0.0f, 1.0f);
        return Transform{
            .position = LerpFinite(a.transform.position, b.transform.position, t),
            .rotationDegrees = LerpFinite(a.transform.rotationDegrees, b.transform.rotationDegrees, t),
            .scale = LerpFinite(a.transform.scale, b.transform.scale, t),
        };
    }

    return sorted.back().transform;
}

} // namespace

void ApplyAnimationClip(Scene& scene, const AnimationClip& clip, double timeSeconds) {
    const double clampedTime = ClampTimeToClip(clip, timeSeconds, clip.looping);
    for (const auto& [nodeHandle, track] : clip.nodeTracks) {
        if (nodeHandle < 0 || static_cast<std::size_t>(nodeHandle) >= scene.NodeCount()) {
            continue;
        }
        scene.GetNode(nodeHandle).localTransform = SampleTrackAtTime(track, clampedTime);
    }
}

AnimationPlayer::AnimationPlayer(const AnimationClip* clip) {
    SetClip(clip);
}

void AnimationPlayer::SetClip(const AnimationClip* clip) {
    clip_ = clip;
    timeSeconds_ = 0.0;
    playing_ = false;
    looping_ = clip_ != nullptr ? clip_->looping : true;
}

void AnimationPlayer::Play(bool restart) {
    if (clip_ == nullptr || !HasUsableDuration(*clip_)) {
        playing_ = false;
        return;
    }
    if (restart) {
        timeSeconds_ = 0.0;
    }
    playing_ = true;
}

void AnimationPlayer::Stop() {
    playing_ = false;
}

void AnimationPlayer::SetLooping(bool looping) {
    looping_ = looping;
    if (clip_ != nullptr) {
        timeSeconds_ = ClampTimeToClip(*clip_, timeSeconds_, looping_);
    }
}

void AnimationPlayer::SetTimeSeconds(double timeSeconds) {
    if (clip_ == nullptr) {
        timeSeconds_ = 0.0;
        return;
    }
    timeSeconds_ = ClampTimeToClip(*clip_, timeSeconds, looping_);
}

void AnimationPlayer::AdvanceSeconds(double deltaSeconds) {
    if (!playing_ || clip_ == nullptr) {
        return;
    }
    if (!HasUsableDuration(*clip_)) {
        timeSeconds_ = 0.0;
        playing_ = false;
        return;
    }
    if (!std::isfinite(deltaSeconds)) {
        return;
    }

    if (looping_) {
        const double wrappedCurrent = ClampTimeToClip(*clip_, timeSeconds_, true);
        const double wrappedDelta = std::fmod(deltaSeconds, clip_->durationSeconds);
        timeSeconds_ = ClampTimeToClip(*clip_, wrappedCurrent + wrappedDelta, true);
        return;
    }

    const double candidate = timeSeconds_ + deltaSeconds;
    if (!std::isfinite(candidate)) {
        timeSeconds_ = deltaSeconds > 0.0 ? clip_->durationSeconds : 0.0;
    } else {
        timeSeconds_ = std::clamp(candidate, 0.0, clip_->durationSeconds);
    }
    if (timeSeconds_ >= clip_->durationSeconds) {
        playing_ = false;
    }
}

bool AnimationPlayer::IsPlaying() const {
    return playing_;
}

double AnimationPlayer::TimeSeconds() const {
    return timeSeconds_;
}

bool AnimationPlayer::Looping() const {
    return looping_;
}

void AnimationPlayer::Apply(Scene& scene) const {
    if (clip_ == nullptr) {
        return;
    }
    const double applyTime = ClampTimeToClip(*clip_, timeSeconds_, looping_);
    for (const auto& [nodeHandle, track] : clip_->nodeTracks) {
        if (nodeHandle < 0 || static_cast<std::size_t>(nodeHandle) >= scene.NodeCount()) {
            continue;
        }
        scene.GetNode(nodeHandle).localTransform = SampleTrackAtTime(track, applyTime);
    }
}

} // namespace ri::scene
