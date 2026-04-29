#include "RawIron/Scene/ScriptedCameraReview.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <utility>

namespace ri::scene {
namespace {

namespace json = ri::core::detail;

[[nodiscard]] ScriptedReviewEase ParseEaseString(const std::string_view text) noexcept {
    if (text == "linear") {
        return ScriptedReviewEase::Linear;
    }
    if (text == "smoothstep") {
        return ScriptedReviewEase::Smoothstep;
    }
    if (text == "easeInOut") {
        return ScriptedReviewEase::EaseInOut;
    }
    return ScriptedReviewEase::Smoothstep;
}

[[nodiscard]] double ApplyInterpolationEase(const double normalizedTime, const ScriptedReviewEase ease) noexcept {
    const double u = std::clamp(normalizedTime, 0.0, 1.0);
    switch (ease) {
        case ScriptedReviewEase::Linear:
            return u;
        case ScriptedReviewEase::Smoothstep:
            return u * u * (3.0 - 2.0 * u);
        case ScriptedReviewEase::EaseInOut:
            return u < 0.5 ? 2.0 * u * u : 1.0 - std::pow(-2.0 * u + 2.0, 2) / 2.0;
        default:
            return u;
    }
}

[[nodiscard]] float LerpYawDegreesShortest(const float fromDegrees, const float toDegrees, const float t) noexcept {
    const float clampedT = std::clamp(t, 0.0f, 1.0f);
    float delta = std::fmod(toDegrees - fromDegrees + 540.0f, 360.0f) - 180.0f;
    return fromDegrees + delta * clampedT;
}

[[nodiscard]] bool ParseOrbitPayload(std::string_view orbitObject, OrbitCameraState& outOrbit, std::string* errorMessage) {
    if (const std::optional<std::string_view> targetObject = json::ExtractJsonObject(orbitObject, "target")) {
        outOrbit.target.x = static_cast<float>(json::ExtractJsonDouble(*targetObject, "x").value_or(0.0));
        outOrbit.target.y = static_cast<float>(json::ExtractJsonDouble(*targetObject, "y").value_or(0.0));
        outOrbit.target.z = static_cast<float>(json::ExtractJsonDouble(*targetObject, "z").value_or(0.0));
    }
    if (const std::optional<double> distance = json::ExtractJsonDouble(orbitObject, "distance")) {
        outOrbit.distance = static_cast<float>(*distance);
    }
    if (const std::optional<double> yaw = json::ExtractJsonDouble(orbitObject, "yawDegrees")) {
        outOrbit.yawDegrees = static_cast<float>(*yaw);
    }
    if (const std::optional<double> pitch = json::ExtractJsonDouble(orbitObject, "pitchDegrees")) {
        outOrbit.pitchDegrees = static_cast<float>(*pitch);
    }
    (void)errorMessage;
    return true;
}

[[nodiscard]] bool ParseStepObject(std::string_view stepObject, ScriptedReviewStep& outStep, std::string* errorMessage) {
    const std::optional<std::string> kindText = json::ExtractJsonString(stepObject, "kind");
    if (!kindText.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = "step missing \"kind\"";
        }
        return false;
    }

    if (*kindText == "wait") {
        outStep.kind = ScriptedReviewStepKind::Wait;
        outStep.durationSeconds = std::max(0.0, json::ExtractJsonDouble(stepObject, "seconds").value_or(0.0));
        return true;
    }

    if (*kindText == "snapOrbit") {
        outStep.kind = ScriptedReviewStepKind::SnapOrbit;
        const std::optional<std::string_view> orbitObj = json::ExtractJsonObject(stepObject, "orbit");
        if (!orbitObj.has_value()) {
            if (errorMessage != nullptr) {
                *errorMessage = "snapOrbit missing \"orbit\" object";
            }
            return false;
        }
        return ParseOrbitPayload(*orbitObj, outStep.orbit, errorMessage);
    }

    if (*kindText == "moveOrbit") {
        outStep.kind = ScriptedReviewStepKind::MoveOrbit;
        outStep.durationSeconds = std::max(0.0, json::ExtractJsonDouble(stepObject, "durationSeconds").value_or(0.0));
        outStep.moveEase = ParseEaseString(json::ExtractJsonString(stepObject, "ease").value_or("smoothstep"));
        const std::optional<std::string_view> orbitObj = json::ExtractJsonObject(stepObject, "orbit");
        if (!orbitObj.has_value()) {
            if (errorMessage != nullptr) {
                *errorMessage = "moveOrbit missing \"orbit\" object";
            }
            return false;
        }
        return ParseOrbitPayload(*orbitObj, outStep.orbit, errorMessage);
    }

    if (*kindText == "frameNodes") {
        outStep.kind = ScriptedReviewStepKind::FrameNodes;
        outStep.frameNodeNames = json::ExtractJsonStringArray(stepObject, "nodeNames");
        if (outStep.frameNodeNames.empty()) {
            if (errorMessage != nullptr) {
                *errorMessage = "frameNodes requires non-empty \"nodeNames\"";
            }
            return false;
        }
        if (const std::optional<double> padding = json::ExtractJsonDouble(stepObject, "padding")) {
            outStep.framePadding = static_cast<float>(*padding);
        }
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = "unknown step kind: " + *kindText;
    }
    return false;
}

} // namespace

void ScriptedCameraSequence::Clear() {
    steps_.clear();
    loopPlayback_ = false;
}

void ScriptedCameraSequence::AddWait(const double seconds) {
    ScriptedReviewStep step{};
    step.kind = ScriptedReviewStepKind::Wait;
    step.durationSeconds = std::max(0.0, seconds);
    steps_.push_back(step);
}

void ScriptedCameraSequence::AddSnapOrbit(const OrbitCameraState& orbit) {
    ScriptedReviewStep step{};
    step.kind = ScriptedReviewStepKind::SnapOrbit;
    step.orbit = orbit;
    steps_.push_back(step);
}

void ScriptedCameraSequence::AddMoveOrbit(const OrbitCameraState& targetOrbit,
                                          const double durationSeconds,
                                          const ScriptedReviewEase ease) {
    ScriptedReviewStep step{};
    step.kind = ScriptedReviewStepKind::MoveOrbit;
    step.durationSeconds = std::max(0.0, durationSeconds);
    step.orbit = targetOrbit;
    step.moveEase = ease;
    steps_.push_back(step);
}

void ScriptedCameraSequence::AddFrameNodes(std::vector<std::string> nodeNames, const float padding) {
    ScriptedReviewStep step{};
    step.kind = ScriptedReviewStepKind::FrameNodes;
    step.frameNodeNames = std::move(nodeNames);
    step.framePadding = std::max(0.01f, padding);
    steps_.push_back(step);
}

ScriptedCameraSequence BuildDefaultStarterSandboxReview() {
    ScriptedCameraSequence sequence{};
    sequence.AddWait(0.5);
    sequence.AddMoveOrbit(
        OrbitCameraState{
            .target = ri::math::Vec3{0.0f, 1.0f, 0.0f},
            .distance = 7.2f,
            .yawDegrees = 205.0f,
            .pitchDegrees = -16.0f,
        },
        2.25);
    sequence.AddMoveOrbit(
        OrbitCameraState{
            .target = ri::math::Vec3{0.4f, 1.1f, 0.2f},
            .distance = 5.5f,
            .yawDegrees = 135.0f,
            .pitchDegrees = -9.0f,
        },
        2.75);
    sequence.AddFrameNodes({"Crate"}, 1.45f);
    sequence.AddSnapOrbit(
        OrbitCameraState{
            .target = ri::math::Vec3{0.0f, 1.0f, 0.0f},
            .distance = 6.0f,
            .yawDegrees = 180.0f,
            .pitchDegrees = -10.0f,
        });
    return sequence;
}

void ScriptedCameraReviewPlayer::SetCompletedCallback(CompletedCallback callback) {
    onCompleted_ = std::move(callback);
}

void ScriptedCameraReviewPlayer::SetStepBeganCallback(StepBeganCallback callback) {
    onStepBegan_ = std::move(callback);
}

void ScriptedCameraReviewPlayer::SetLoopPlayback(const bool loop) noexcept {
    loopPlayback_ = loop;
}

void ScriptedCameraReviewPlayer::Start(ScriptedCameraSequence sequence) {
    sequence_ = std::move(sequence);
    loopPlayback_ = sequence_.LoopPlayback();
    stepIndex_ = 0;
    elapsedInStep_ = 0.0;
    moveHasStart_ = false;
    completed_ = false;
    completedLoopCount_ = 0;
    lastAnnouncedStep_.reset();
    active_ = !sequence_.Steps().empty();
    if (!active_) {
        completed_ = true;
        if (onCompleted_) {
            onCompleted_();
        }
    }
}

void ScriptedCameraReviewPlayer::Stop() {
    active_ = false;
    completed_ = false;
    stepIndex_ = 0;
    elapsedInStep_ = 0.0;
    moveHasStart_ = false;
    completedLoopCount_ = 0;
    lastAnnouncedStep_.reset();
}

void ScriptedCameraReviewPlayer::AdvanceStep() {
    stepIndex_ += 1;
    elapsedInStep_ = 0.0;
    moveHasStart_ = false;
    if (stepIndex_ >= sequence_.Steps().size()) {
        if (loopPlayback_ && !sequence_.Steps().empty()) {
            stepIndex_ = 0;
            completedLoopCount_ += 1;
            lastAnnouncedStep_.reset();
            return;
        }
        active_ = false;
        completed_ = true;
        if (onCompleted_) {
            onCompleted_();
        }
    }
}

std::string_view ScriptedCameraReviewPlayer::StepKindName(const ScriptedReviewStepKind kind) noexcept {
    switch (kind) {
        case ScriptedReviewStepKind::Wait:
            return "wait";
        case ScriptedReviewStepKind::SnapOrbit:
            return "snapOrbit";
        case ScriptedReviewStepKind::MoveOrbit:
            return "moveOrbit";
        case ScriptedReviewStepKind::FrameNodes:
            return "frameNodes";
    }
    return "unknown";
}

void ScriptedCameraReviewPlayer::NotifyStepBeganIfNeeded(const ScriptedReviewStep& step) {
    if (!onStepBegan_) {
        return;
    }
    if (lastAnnouncedStep_.has_value() && *lastAnnouncedStep_ == stepIndex_) {
        return;
    }
    lastAnnouncedStep_ = stepIndex_;
    onStepBegan_(stepIndex_, step.kind, StepKindName(step.kind));
}

OrbitCameraState ScriptedCameraReviewPlayer::LerpOrbit(const OrbitCameraState& from,
                                                       const OrbitCameraState& to,
                                                       const double t) const {
    const float ft = static_cast<float>(std::clamp(t, 0.0, 1.0));
    OrbitCameraState result{};
    result.target.x = from.target.x + (to.target.x - from.target.x) * ft;
    result.target.y = from.target.y + (to.target.y - from.target.y) * ft;
    result.target.z = from.target.z + (to.target.z - from.target.z) * ft;
    result.distance = from.distance + (to.distance - from.distance) * ft;
    result.yawDegrees = LerpYawDegreesShortest(from.yawDegrees, to.yawDegrees, ft);
    result.pitchDegrees = from.pitchDegrees + (to.pitchDegrees - from.pitchDegrees) * ft;
    return result;
}

bool ScriptedCameraReviewPlayer::Tick(Scene& scene, OrbitCameraHandles& orbit, const double deltaSeconds) {
    if (!active_) {
        return false;
    }

    const double delta = std::isfinite(deltaSeconds) ? std::max(0.0, deltaSeconds) : 0.0;
    if (stepIndex_ >= sequence_.Steps().size()) {
        active_ = false;
        return false;
    }

    const ScriptedReviewStep& step = sequence_.Steps()[stepIndex_];
    NotifyStepBeganIfNeeded(step);

    switch (step.kind) {
        case ScriptedReviewStepKind::Wait: {
            elapsedInStep_ += delta;
            if (elapsedInStep_ >= step.durationSeconds) {
                AdvanceStep();
            }
            break;
        }
        case ScriptedReviewStepKind::SnapOrbit: {
            SetOrbitCameraState(scene, orbit, step.orbit);
            AdvanceStep();
            break;
        }
        case ScriptedReviewStepKind::MoveOrbit: {
            if (!moveHasStart_) {
                moveStartOrbit = orbit.orbit;
                moveHasStart_ = true;
            }
            elapsedInStep_ += delta;
            const double duration = std::max(step.durationSeconds, 1.0e-4);
            const double alpha = std::clamp(elapsedInStep_ / duration, 0.0, 1.0);
            const double eased = ApplyInterpolationEase(alpha, step.moveEase);
            const OrbitCameraState blended = LerpOrbit(moveStartOrbit, step.orbit, eased);
            SetOrbitCameraState(scene, orbit, blended);
            if (alpha >= 1.0 - 1.0e-6) {
                AdvanceStep();
            }
            break;
        }
        case ScriptedReviewStepKind::FrameNodes: {
            std::vector<int> handles;
            handles.reserve(step.frameNodeNames.size());
            for (const std::string& name : step.frameNodeNames) {
                const int found = FindFirstNodeNamed(scene, name);
                if (found != kInvalidHandle) {
                    handles.push_back(found);
                }
            }
            if (!handles.empty()) {
                (void)FrameNodesWithOrbitCamera(scene, orbit, handles, step.framePadding);
            }
            AdvanceStep();
            break;
        }
    }

    return active_;
}

int FindFirstNodeNamed(const Scene& scene, std::string_view name) {
    const int count = static_cast<int>(scene.NodeCount());
    for (int index = 0; index < count; ++index) {
        if (scene.GetNode(index).name == name) {
            return index;
        }
    }
    return kInvalidHandle;
}

bool TryParseScriptedCameraSequenceFromJson(const std::string_view jsonText,
                                            ScriptedCameraSequence& outSequence,
                                            std::string* errorMessage) {
    outSequence.Clear();
    const std::optional<bool> loopFlag = json::ExtractJsonBool(jsonText, "loop");
    if (loopFlag.has_value()) {
        outSequence.SetLoopPlayback(*loopFlag);
    }
    const std::optional<std::int32_t> formatVersion = json::ExtractJsonInt(jsonText, "formatVersion");
    if (formatVersion.has_value() && *formatVersion != 1) {
        if (errorMessage != nullptr) {
            std::ostringstream stream;
            stream << "unsupported formatVersion " << *formatVersion;
            *errorMessage = stream.str();
        }
        return false;
    }

    const std::vector<std::string_view> stepObjects = json::SplitJsonArrayObjects(jsonText, "steps");
    if (stepObjects.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "missing or empty \"steps\" array";
        }
        return false;
    }

    for (const std::string_view stepText : stepObjects) {
        ScriptedReviewStep step{};
        if (!ParseStepObject(stepText, step, errorMessage)) {
            return false;
        }
        switch (step.kind) {
            case ScriptedReviewStepKind::Wait:
                outSequence.AddWait(step.durationSeconds);
                break;
            case ScriptedReviewStepKind::SnapOrbit:
                outSequence.AddSnapOrbit(step.orbit);
                break;
            case ScriptedReviewStepKind::MoveOrbit:
                outSequence.AddMoveOrbit(step.orbit, step.durationSeconds, step.moveEase);
                break;
            case ScriptedReviewStepKind::FrameNodes:
                outSequence.AddFrameNodes(std::move(step.frameNodeNames), step.framePadding);
                break;
        }
    }

    return true;
}

bool TryLoadScriptedCameraSequenceFromJsonFile(const std::filesystem::path& path,
                                               ScriptedCameraSequence& outSequence,
                                               std::string* errorMessage) {
    const std::string text = json::ReadTextFile(path);
    if (text.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "failed to read scripted camera JSON or file empty";
        }
        return false;
    }
    return TryParseScriptedCameraSequenceFromJson(text, outSequence, errorMessage);
}

} // namespace ri::scene