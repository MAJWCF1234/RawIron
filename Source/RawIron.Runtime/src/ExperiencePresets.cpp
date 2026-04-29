#include "RawIron/Runtime/ExperiencePresets.h"

namespace ri::runtime {

std::optional<ExperiencePresetPatch> ResolveExperiencePreset(std::string_view presetId) {
    if (presetId == "rawiron-signature") {
        ExperiencePresetPatch patch{};
        patch.gameId = "liminal-hall";
        patch.width = 1920;
        patch.height = 1080;
        patch.windowTitle = "RawIron Signature Experience";
        patch.defaultHeadlessOutputRelativePath = std::filesystem::path("Saved/Previews/rawiron_signature_headless.bmp");
        return patch;
    }
    return std::nullopt;
}

std::vector<std::string_view> SupportedExperiencePresets() {
    return {"rawiron-signature"};
}

} // namespace ri::runtime
