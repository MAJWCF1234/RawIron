#include "RawIron/Ui/UiFlowSession.h"

#include <algorithm>

namespace ri::ui {

void UiFlowSession::Reset(const UiManifest& manifest) {
    manifest_ = &manifest;
    screenIndex_.clear();
    stack_.clear();
    variables_.clear();
    history_.clear();
    historyFingerprints_.clear();
    historyFingerprintOrder_.clear();
    ++navigationRevision_;
    for (std::size_t index = 0; index < manifest.screens.size(); ++index) {
        if (!manifest.screens[index].id.empty()) {
            screenIndex_.try_emplace(manifest.screens[index].id, index);
        }
    }
    for (const UiVariableDef& def : manifest.variables) {
        if (!def.id.empty()) {
            variables_[def.id] = def.value;
        }
    }
    if (!manifest.startScreenId.empty()) {
        stack_.push_back(manifest.startScreenId);
    } else if (!manifest.screens.empty()) {
        stack_.push_back(manifest.screens.front().id);
    }
}

const UiScreen* UiFlowSession::CurrentScreen() const {
    if (manifest_ == nullptr || stack_.empty()) {
        return nullptr;
    }
    const auto found = screenIndex_.find(stack_.back());
    return found == screenIndex_.end() || found->second >= manifest_->screens.size()
        ? nullptr
        : &manifest_->screens[found->second];
}

bool UiFlowSession::NavigateTo(const std::string_view screenId) {
    if (manifest_ == nullptr) {
        return false;
    }
    if (!screenIndex_.contains(screenId)) {
        return false;
    }
    constexpr std::size_t kMaxNavigationDepth = 128U;
    if (stack_.size() >= kMaxNavigationDepth) {
        // Preserve the root destination while discarding the oldest intermediate visit.
        stack_.erase(stack_.begin() + 1);
    }
    stack_.push_back(std::string(screenId));
    ++navigationRevision_;
    return true;
}

bool UiFlowSession::GoBack() {
    if (stack_.size() <= 1U) {
        return false;
    }
    stack_.pop_back();
    ++navigationRevision_;
    return true;
}

std::string UiFlowSession::GetVariableValue(std::string_view id) const {
    return std::string(GetVariableValueView(id));
}

std::string_view UiFlowSession::GetVariableValueView(const std::string_view id) const noexcept {
    const auto it = variables_.find(id);
    return it == variables_.end() ? std::string_view{} : std::string_view{it->second};
}

bool UiFlowSession::IsBlockVisible(const UiBlock& block) const {
    if (block.visibleWhenVar.empty()) {
        return true;
    }
    return GetVariableValueView(block.visibleWhenVar) == block.visibleWhenEquals;
}

bool UiFlowSession::IsChoiceVisible(const UiChoiceItem& choice) const {
    if (choice.visibleWhenVar.empty()) {
        return true;
    }
    return GetVariableValueView(choice.visibleWhenVar) == choice.visibleWhenEquals;
}

bool UiFlowSession::ActionWhenAllows(const UiAction& action) const {
    if (action.whenVar.empty()) {
        return true;
    }
    return GetVariableValueView(action.whenVar) == action.whenEquals;
}

void UiFlowSession::ApplyBundledSetVar(const UiAction& action) {
    if (!action.setVarId.empty()) {
        variables_[action.setVarId] = action.setVarValue;
    }
}

void UiFlowSession::MaybeAppendHistory(std::string_view fingerprint, UiHistoryLine line) {
    if (fingerprint.empty()) {
        return;
    }
    const std::string key(fingerprint);
    if (!historyFingerprints_.insert(key).second) {
        return;
    }
    history_.push_back(std::move(line));
    constexpr std::size_t kMaxHistory = 160U;
    if (history_.size() > kMaxHistory) {
        const std::size_t overflow = history_.size() - kMaxHistory;
        history_.erase(
            history_.begin(),
            history_.begin() + static_cast<std::vector<UiHistoryLine>::difference_type>(overflow));
    }
    historyFingerprintOrder_.push_back(key);
    constexpr std::size_t kMaxRememberedFingerprints = 320U;
    while (historyFingerprintOrder_.size() > kMaxRememberedFingerprints) {
        historyFingerprints_.erase(historyFingerprintOrder_.front());
        historyFingerprintOrder_.pop_front();
    }
}

bool UiFlowSession::ApplyAction(const UiAction& action,
                                const std::function<void(std::string_view actionId)>& onEmit) {
    if (!ActionWhenAllows(action)) {
        return false;
    }
    switch (action.kind) {
        case UiActionKind::None:
            return false;
        case UiActionKind::SetVariable:
            if (!action.target.empty()) {
                variables_[action.target] = action.value;
            }
            return true;
        case UiActionKind::Navigate:
            ApplyBundledSetVar(action);
            return NavigateTo(action.target);
        case UiActionKind::Back:
            ApplyBundledSetVar(action);
            return GoBack();
        case UiActionKind::Emit:
            ApplyBundledSetVar(action);
            if (onEmit) {
                onEmit(action.target);
            }
            return true;
    }
    return false;
}

} // namespace ri::ui
