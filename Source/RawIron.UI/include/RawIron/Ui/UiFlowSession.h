#pragma once

#include "RawIron/Ui/UiManifest.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ri::ui {

/// Stack-based navigation over a loaded `UiManifest` (JSON-defined screens).
class UiFlowSession {
public:
    void Reset(const UiManifest& manifest);

    [[nodiscard]] const UiManifest* Manifest() const noexcept { return manifest_; }

    [[nodiscard]] const UiScreen* CurrentScreen() const;

    bool NavigateTo(std::string_view screenId);
    bool GoBack();

    /// Applies a button/menu action: navigate, back stack pop, `setVar`, or `onEmit` for custom ids.
    bool ApplyAction(const UiAction& action, const std::function<void(std::string_view actionId)>& onEmit);

    /// Ren'Py-style store lookup (empty if unset).
    [[nodiscard]] std::string GetVariableValue(std::string_view id) const;

    /// Hides blocks when `visibleWhen` does not match the current store.
    [[nodiscard]] bool IsBlockVisible(const UiBlock& block) const;

    [[nodiscard]] bool IsChoiceVisible(const UiChoiceItem& choice) const;

    /// Append one backlog line if `fingerprint` was not recorded this session (VN / Ren'Py history).
    void MaybeAppendHistory(std::string_view fingerprint, UiHistoryLine line);

    [[nodiscard]] const std::vector<UiHistoryLine>& History() const noexcept { return history_; }

    [[nodiscard]] const std::vector<std::string>& Stack() const noexcept { return stack_; }

private:
    [[nodiscard]] bool ActionWhenAllows(const UiAction& action) const;
    void ApplyBundledSetVar(const UiAction& action);

    const UiManifest* manifest_ = nullptr;
    std::vector<std::string> stack_{};
    std::unordered_map<std::string, std::string> variables_{};
    std::vector<UiHistoryLine> history_{};
    std::unordered_set<std::string> historyFingerprints_{};
};

} // namespace ri::ui
