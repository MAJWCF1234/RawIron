#pragma once

#include "RawIron/Scene/Scene.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ri::scene {

struct TransformKeyframe {
    double timeSeconds = 0.0;
    Transform transform{};
};

struct AnimationClip {
    std::string name;
    double durationSeconds = 0.0;
    bool looping = true;
    std::unordered_map<int, std::vector<TransformKeyframe>> nodeTracks;
};

void ApplyAnimationClip(Scene& scene, const AnimationClip& clip, double timeSeconds);

class AnimationPlayer {
public:
    AnimationPlayer() = default;
    explicit AnimationPlayer(const AnimationClip* clip);

    void SetClip(const AnimationClip* clip);
    void Play(bool restart = false);
    void Stop();
    void SetLooping(bool looping);
    void SetTimeSeconds(double timeSeconds);
    void AdvanceSeconds(double deltaSeconds);

    [[nodiscard]] bool IsPlaying() const;
    [[nodiscard]] double TimeSeconds() const;
    [[nodiscard]] bool Looping() const;

    void Apply(Scene& scene) const;

private:
    const AnimationClip* clip_ = nullptr;
    double timeSeconds_ = 0.0;
    bool looping_ = true;
    bool playing_ = false;
};

} // namespace ri::scene
