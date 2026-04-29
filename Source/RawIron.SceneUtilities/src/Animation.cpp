#include "RawIron/Scene/Animation.h"

#include "RawIron/Math/Vec3.h"

#include <algorithm>
#include <cmath>

namespace ri::scene {

namespace {

double ClampTimeToClip(const AnimationClip& clip, double timeSeconds, bool looping) {
    if (clip.durationSeconds <= 0.0) {
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
    if (track.empty()) {
        return Transform{};
    }

    std::vector<TransformKeyframe> sorted = track;
    std::sort(sorted.begin(), sorted.end(), [](const TransformKeyframe& lhs, const TransformKeyframe& rhs) {
        return lhs.timeSeconds < rhs.timeSeconds;
    });

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

        const double span = std::max(1.0e-8, b.timeSeconds - a.timeSeconds);
        const float t = static_cast<float>((timeSeconds - a.timeSeconds) / span);
        return Transform{
            .position = ri::math::Lerp(a.transform.position, b.transform.position, t),
            .rotationDegrees = ri::math::Lerp(a.transform.rotationDegrees, b.transform.rotationDegrees, t),
            .scale = ri::math::Lerp(a.transform.scale, b.transform.scale, t),
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
    if (clip_ == nullptr) {
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
    if (!std::isfinite(deltaSeconds)) {
        return;
    }

    if (looping_) {
        timeSeconds_ = ClampTimeToClip(*clip_, timeSeconds_ + deltaSeconds, true);
        return;
    }

    timeSeconds_ = std::clamp(timeSeconds_ + deltaSeconds, 0.0, clip_->durationSeconds);
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
