#pragma once

#include "RawIron/Runtime/RuntimeEventBus.h"
#include "RawIron/Runtime/RuntimeId.h"
#include "RawIron/World/PresentationState.h"

#include <string>
#include <string_view>

namespace ri::world::text_overlay_events {

inline constexpr std::string_view kEventMessage = "message";
inline constexpr std::string_view kEventSubtitle = "subtitle";
inline constexpr std::string_view kEventLevelToast = "levelToast";
inline constexpr std::string_view kEventObjectiveChanged = "objectiveChanged";
inline constexpr std::string_view kEventLoadingProgress = "loadingProgress";
inline constexpr std::string_view kEventVoiceLine = "voiceLine";
inline constexpr std::string_view kEventVoiceStop = "voiceStop";

inline std::string SeverityToString(PresentationSeverity severity) {
    return severity == PresentationSeverity::Critical ? "critical" : "normal";
}

inline void EmitMessage(ri::runtime::RuntimeEventBus& bus,
                        std::string text,
                        double durationMs = 4000.0,
                        PresentationSeverity severity = PresentationSeverity::Normal) {
    bus.Emit(kEventMessage,
             ri::runtime::RuntimeEvent{
                 .id = ri::runtime::CreateRuntimeId("overlay_msg"),
                 .type = std::string(kEventMessage),
                 .fields = {
                     {"text", std::move(text)},
                     {"durationMs", std::to_string(durationMs)},
                     {"severity", SeverityToString(severity)},
                 },
             });
}

inline void EmitSubtitle(ri::runtime::RuntimeEventBus& bus, std::string text, double durationMs = 5000.0) {
    bus.Emit(kEventSubtitle,
             ri::runtime::RuntimeEvent{
                 .id = ri::runtime::CreateRuntimeId("overlay_sub"),
                 .type = std::string(kEventSubtitle),
                 .fields = {
                     {"text", std::move(text)},
                     {"durationMs", std::to_string(durationMs)},
                 },
             });
}

inline void EmitLevelToast(ri::runtime::RuntimeEventBus& bus, std::string text, double durationMs = 2500.0) {
    bus.Emit(kEventLevelToast,
             ri::runtime::RuntimeEvent{
                 .id = ri::runtime::CreateRuntimeId("overlay_toast"),
                 .type = std::string(kEventLevelToast),
                 .fields = {
                     {"text", std::move(text)},
                     {"durationMs", std::to_string(durationMs)},
                 },
             });
}

inline void EmitObjectiveChanged(ri::runtime::RuntimeEventBus& bus,
                                 std::string text,
                                 bool announce = true,
                                 bool flash = true,
                                 std::string hint = {},
                                 double hintDurationMs = 7000.0) {
    bus.Emit(kEventObjectiveChanged,
             ri::runtime::RuntimeEvent{
                 .id = ri::runtime::CreateRuntimeId("overlay_obj"),
                 .type = std::string(kEventObjectiveChanged),
                 .fields = {
                     {"text", std::move(text)},
                     {"announce", announce ? "true" : "false"},
                     {"flash", flash ? "true" : "false"},
                     {"hint", std::move(hint)},
                     {"hintDurationMs", std::to_string(hintDurationMs)},
                 },
             });
}

inline void EmitLoadingProgress(ri::runtime::RuntimeEventBus& bus,
                                bool visible,
                                double progress01,
                                std::string status = {}) {
    bus.Emit(kEventLoadingProgress,
             ri::runtime::RuntimeEvent{
                 .id = ri::runtime::CreateRuntimeId("overlay_loading"),
                 .type = std::string(kEventLoadingProgress),
                 .fields = {
                     {"visible", visible ? "true" : "false"},
                     {"progress01", std::to_string(progress01)},
                     {"status", std::move(status)},
                 },
             });
}

inline void EmitVoiceLine(ri::runtime::RuntimeEventBus& bus,
                          std::string audioPath,
                          std::string subtitleText,
                          std::string speaker = {},
                          double volume = 1.0,
                          double subtitleDurationMs = 0.0) {
    bus.Emit(kEventVoiceLine,
             ri::runtime::RuntimeEvent{
                 .id = ri::runtime::CreateRuntimeId("overlay_voice"),
                 .type = std::string(kEventVoiceLine),
                 .fields = {
                     {"audioPath", std::move(audioPath)},
                     {"subtitleText", std::move(subtitleText)},
                     {"speaker", std::move(speaker)},
                     {"volume", std::to_string(volume)},
                     {"subtitleDurationMs", std::to_string(subtitleDurationMs)},
                 },
             });
}

inline void EmitVoiceStop(ri::runtime::RuntimeEventBus& bus) {
    bus.Emit(kEventVoiceStop,
             ri::runtime::RuntimeEvent{
                 .id = ri::runtime::CreateRuntimeId("overlay_voice_stop"),
                 .type = std::string(kEventVoiceStop),
                 .fields = {},
             });
}

} // namespace ri::world::text_overlay_events
