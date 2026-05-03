#include "RawIron/Ui/UiFlowSession.h"

#include <algorithm>

namespace ri::ui {

void UiFlowSession::Reset(const UiManifest& manifest) {
    manifest_ = &manifest;
    stack_.clear();
    variables_.clear();
    history_.clear();
    historyFingerprints_.clear();
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
    const std::string& top = stack_.back();
    for (const UiScreen& s : manifest_->screens) {
        if (s.id == top) {
            return &s;
        }
    }
    return nullptr;
}

bool UiFlowSession::NavigateTo(const std::string_view screenId) {
    if (manifest_ == nullptr) {
        return false;
    }
    for (const UiScreen& s : manifest_->screens) {
        if (s.id == screenId) {
            stack_.push_back(std::string(screenId));
            return true;
        }
    }
    return false;
}

bool UiFlowSession::GoBack() {
    if (stack_.size() <= 1U) {
        return false;
    }
    stack_.pop_back();
    return true;
}

std::string UiFlowSession::GetVariableValue(std::string_view id) const {
    const std::string key(id);
    const auto it = variables_.find(key);
    if (it == variables_.end()) {
        return {};
    }
    return it->second;
}

bool UiFlowSession::IsBlockVisible(const UiBlock& block) const {
    if (block.visibleWhenVar.empty()) {
        return true;
    }
    return GetVariableValue(block.visibleWhenVar) == block.visibleWhenEquals;
}

bool UiFlowSession::IsChoiceVisible(const UiChoiceItem& choice) const {
    if (choice.visibleWhenVar.empty()) {
        return true;
    }
    return GetVariableValue(choice.visibleWhenVar) == choice.visibleWhenEquals;
}

bool UiFlowSession::ActionWhenAllows(const UiAction& action) const {
    if (action.whenVar.empty()) {
        return true;
    }
    return GetVariableValue(action.whenVar) == action.whenEquals;
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
    while (history_.size() > kMaxHistory) {
        history_.erase(history_.begin());
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
