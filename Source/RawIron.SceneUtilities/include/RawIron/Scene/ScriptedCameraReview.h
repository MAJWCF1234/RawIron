#pragma once

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/Scene.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::scene {

enum class ScriptedReviewStepKind : std::uint8_t {
    Wait,
    SnapOrbit,
    MoveOrbit,
    FrameNodes,
};

/// Interpolation mode for `moveOrbit` (applied to the eased time parameter in `[0,1]`).
enum class ScriptedReviewEase : std::uint8_t {
    Linear,
    Smoothstep,
    EaseInOut,
};

/// One step in a scripted camera review sequence (orbit rig only — development / inspection tool).
struct ScriptedReviewStep {
    ScriptedReviewStepKind kind = ScriptedReviewStepKind::Wait;
    double durationSeconds = 0.0;
    OrbitCameraState orbit{};
    ScriptedReviewEase moveEase = ScriptedReviewEase::Smoothstep;
    std::vector<std::string> frameNodeNames{};
    float framePadding = 1.35f;
};

/// Builds a scripted sequence programmatically or via `TryLoadScriptedCameraSequenceFromJsonFile`.
class ScriptedCameraSequence {
public:
    void Clear();
    void AddWait(double seconds);
    void AddSnapOrbit(const OrbitCameraState& orbit);
    void AddMoveOrbit(const OrbitCameraState& targetOrbit,
                      double durationSeconds,
                      ScriptedReviewEase ease = ScriptedReviewEase::Smoothstep);
    /// Frame the listed scene node names once (resolved at step execution time).
    void AddFrameNodes(std::vector<std::string> nodeNames, float padding = 1.35f);

    [[nodiscard]] const std::vector<ScriptedReviewStep>& Steps() const noexcept { return steps_; }

    /// When true (also supported as top-level JSON `"loop": true`), the player should restart the sequence after the last step.
    void SetLoopPlayback(bool loop) noexcept { loopPlayback_ = loop; }
    [[nodiscard]] bool LoopPlayback() const noexcept { return loopPlayback_; }

private:
    std::vector<ScriptedReviewStep> steps_{};
    bool loopPlayback_ = false;
};

/// Runs a sequence by updating an orbit camera rig each tick. Optional callback when the sequence completes.
class ScriptedCameraReviewPlayer {
public:
    using CompletedCallback = std::function<void()>;
    /// Fired the first time a step becomes active each time it is entered (including after a loop restart).
    using StepBeganCallback =
        std::function<void(std::size_t stepIndex, ScriptedReviewStepKind kind, std::string_view kindName)>;

    void Start(ScriptedCameraSequence sequence);
    void Stop();
    void SetCompletedCallback(CompletedCallback callback);
    /// Restarts from step 0 when the sequence ends (mutually exclusive with `Completed()` until `Stop()`).
    void SetLoopPlayback(bool loop) noexcept;
    [[nodiscard]] bool LoopPlayback() const noexcept { return loopPlayback_; }
    /// Full loops finished (only advances when loop playback is enabled).
    [[nodiscard]] std::uint64_t CompletedLoopCount() const noexcept { return completedLoopCount_; }
    void SetStepBeganCallback(StepBeganCallback callback);

    [[nodiscard]] bool IsActive() const noexcept { return active_; }
    [[nodiscard]] bool Completed() const noexcept { return completed_; }
    [[nodiscard]] std::size_t CurrentStepIndex() const noexcept { return stepIndex_; }
    [[nodiscard]] double StepElapsedSeconds() const noexcept { return elapsedInStep_; }

    /// Advances playback; returns `true` while the scripted review is still running.
    bool Tick(Scene& scene, OrbitCameraHandles& orbit, double deltaSeconds);

private:
    void AdvanceStep();
    [[nodiscard]] OrbitCameraState LerpOrbit(const OrbitCameraState& from,
                                             const OrbitCameraState& to,
                                             double t) const;
    void NotifyStepBeganIfNeeded(const ScriptedReviewStep& step);
    [[nodiscard]] static std::string_view StepKindName(ScriptedReviewStepKind kind) noexcept;

    ScriptedCameraSequence sequence_{};
    std::size_t stepIndex_ = 0;
    double elapsedInStep_ = 0.0;
    bool active_ = false;
    bool completed_ = false;
    bool loopPlayback_ = false;
    std::uint64_t completedLoopCount_ = 0;
    std::optional<std::size_t> lastAnnouncedStep_;
    OrbitCameraState moveStartOrbit{};
    bool moveHasStart_ = false;
    CompletedCallback onCompleted_{};
    StepBeganCallback onStepBegan_{};
};

[[nodiscard]] ScriptedCameraSequence BuildDefaultStarterSandboxReview();

[[nodiscard]] bool TryParseScriptedCameraSequenceFromJson(std::string_view jsonText,
                                                          ScriptedCameraSequence& outSequence,
                                                          std::string* errorMessage);

[[nodiscard]] bool TryLoadScriptedCameraSequenceFromJsonFile(const std::filesystem::path& path,
                                                             ScriptedCameraSequence& outSequence,
                                                             std::string* errorMessage);

/// Finds the first scene node whose `Node::name` matches (linear scan).
[[nodiscard]] int FindFirstNodeNamed(const Scene& scene, std::string_view name);

} // namespace ri::scene
