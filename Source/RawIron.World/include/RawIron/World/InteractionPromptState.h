#pragma once

#include <cstdint>
#include <string>

namespace ri::world {

enum class InteractionPromptSuppression : std::uint8_t {
    None = 0,
    Paused = 1 << 0,
    Inventory = 1 << 1,
    DebugOverlay = 1 << 2,
    InputUnlocked = 1 << 3,
};

struct InteractionPromptRequest {
    std::string actionLabel = "E";
    std::string verb = "INTERACT";
    std::string targetLabel;
    std::string fallbackLabel;
};

struct InteractionPromptView {
    bool visible = false;
    std::string text;
    std::string actionLabel;
    std::string verb;
    std::string targetLabel;
};

class InteractionPromptState {
public:
    void Show(InteractionPromptRequest request);
    void Clear() noexcept;

    void SetSuppressed(InteractionPromptSuppression suppression, bool enabled) noexcept;
    [[nodiscard]] bool Suppressed() const noexcept;
    [[nodiscard]] std::uint32_t SuppressionMask() const noexcept;

    [[nodiscard]] InteractionPromptView BuildView() const;

private:
    [[nodiscard]] static std::string NormalizeLabel(std::string text, bool uppercase);

    InteractionPromptRequest request_{};
    std::uint32_t suppressionMask_ = 0U;
    bool hasPrompt_ = false;
};

} // namespace ri::world
