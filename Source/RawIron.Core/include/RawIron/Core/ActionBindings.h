#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::core {

enum class BindingSlot : std::uint8_t {
    Primary = 0,
    Secondary = 1,
};

struct ActionBinding {
    std::string actionId;
    std::string displayName;
    std::string primaryInputId;
    std::string secondaryInputId;
    bool allowRebind = true;
};

class ActionBindings {
public:
    ActionBindings() = default;
    explicit ActionBindings(std::vector<ActionBinding> bindings);

    bool DefineAction(ActionBinding binding);
    bool Rebind(std::string_view actionId,
                std::string_view inputId,
                BindingSlot slot = BindingSlot::Primary,
                bool clearConflicts = true);
    bool ClearBinding(std::string_view actionId, BindingSlot slot) noexcept;

    [[nodiscard]] const ActionBinding* FindAction(std::string_view actionId) const noexcept;
    [[nodiscard]] ActionBinding* FindAction(std::string_view actionId) noexcept;
    [[nodiscard]] std::optional<std::string> ResolveAction(std::string_view inputId) const;
    [[nodiscard]] const ActionBinding* FindBoundAction(std::string_view inputId) const noexcept;
    [[nodiscard]] std::vector<ActionBinding> Snapshot() const;

    [[nodiscard]] static std::string FormatInputLabel(std::string_view inputId);

private:
    [[nodiscard]] static std::string NormalizeInputId(std::string_view inputId);
    [[nodiscard]] bool BindingExists(std::string_view inputId, const ActionBinding* ignore) const noexcept;
    void ClearConflictingBindings(std::string_view inputId,
                                  BindingSlot claimedSlot,
                                  const ActionBinding* owner) noexcept;

    std::vector<ActionBinding> bindings_{};
};

} // namespace ri::core
