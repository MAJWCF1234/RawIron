#include "RawIron/Trace/LocomotionRuntimeBridge.h"

#include "RawIron/Runtime/RuntimeTuning.h"

#include <optional>
#include <string>

namespace ri::trace {
namespace {

std::optional<float> FloatPatch(std::string_view key,
                                const std::unordered_map<std::string, double>& record) noexcept {
    const auto it = record.find(std::string(key));
    const std::optional<double> raw =
        it == record.end() ? std::nullopt : std::optional<double>(it->second);
    const std::optional<double> sanitized = ri::runtime::SanitizeRuntimeTuningValue(key, raw);
    if (!sanitized.has_value()) {
        return std::nullopt;
    }
    return static_cast<float>(*sanitized);
}

void ApplyOptionalPatch(std::string_view key,
                        std::optional<float>& field,
                        const std::unordered_map<std::string, double>& record) noexcept {
    if (const std::optional<float> value = FloatPatch(key, record)) {
        field = value;
    }
}

} // namespace

LocomotionTuning LocomotionTuningFromRuntimeRecord(const std::unordered_map<std::string, double>& record) noexcept {
    LocomotionTuning base = DefaultLocomotionTuning();
    LocomotionTuningPatch patch{};
    ApplyOptionalPatch("walkSpeed", patch.walkSpeed, record);
    ApplyOptionalPatch("sprintSpeed", patch.sprintSpeed, record);
    ApplyOptionalPatch("crouchSpeed", patch.crouchSpeed, record);
    ApplyOptionalPatch("proneSpeed", patch.proneSpeed, record);
    ApplyOptionalPatch("gravity", patch.gravity, record);
    ApplyOptionalPatch("jumpForce", patch.jumpForce, record);
    ApplyOptionalPatch("fallGravityMultiplier", patch.fallGravityMultiplier, record);
    ApplyOptionalPatch("lowJumpGravityMultiplier", patch.lowJumpGravityMultiplier, record);
    ApplyOptionalPatch("maxFallSpeed", patch.maxFallSpeed, record);
    ApplyOptionalPatch("maxStepHeight", patch.maxStepHeight, record);
    ApplyLocomotionTuningPatch(base, patch);
    return base;
}

std::unordered_map<std::string, double> LocomotionTuningToRuntimeRecordSubset(const LocomotionTuning& tuning) noexcept {
    std::unordered_map<std::string, double> out{};
    out.reserve(13);
    out["walkSpeed"] = static_cast<double>(tuning.walkSpeed);
    out["sprintSpeed"] = static_cast<double>(tuning.sprintSpeed);
    out["crouchSpeed"] = static_cast<double>(tuning.crouchSpeed);
    out["proneSpeed"] = static_cast<double>(tuning.proneSpeed);
    out["gravity"] = static_cast<double>(tuning.gravity);
    out["jumpForce"] = static_cast<double>(tuning.jumpForce);
    out["fallGravityMultiplier"] = static_cast<double>(tuning.fallGravityMultiplier);
    out["lowJumpGravityMultiplier"] = static_cast<double>(tuning.lowJumpGravityMultiplier);
    out["maxFallSpeed"] = static_cast<double>(tuning.maxFallSpeed);
    out["maxStepHeight"] = static_cast<double>(tuning.maxStepHeight);
    return out;
}

} // namespace ri::trace
