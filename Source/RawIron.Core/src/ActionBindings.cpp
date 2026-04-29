#include "RawIron/Core/ActionBindings.h"
#include "RawIron/Core/InputLabelFormat.h"

#include <algorithm>

namespace ri::core {

ActionBindings::ActionBindings(std::vector<ActionBinding> bindings) {
    for (ActionBinding& binding : bindings) {
        DefineAction(std::move(binding));
    }
}

bool ActionBindings::DefineAction(ActionBinding binding) {
    if (binding.actionId.empty()) {
        return false;
    }

    binding.primaryInputId = NormalizeInputId(binding.primaryInputId);
    binding.secondaryInputId = NormalizeInputId(binding.secondaryInputId);

    ActionBinding* existing = FindAction(binding.actionId);
    if (existing != nullptr) {
        *existing = std::move(binding);
        return true;
    }

    bindings_.push_back(std::move(binding));
    return true;
}

bool ActionBindings::Rebind(std::string_view actionId,
                            std::string_view inputId,
                            BindingSlot slot,
                            bool clearConflicts) {
    ActionBinding* binding = FindAction(actionId);
    if (binding == nullptr || !binding->allowRebind) {
        return false;
    }

    const std::string normalizedInput = NormalizeInputId(inputId);
    if (normalizedInput.empty()) {
        return false;
    }

    if (clearConflicts) {
        ClearConflictingBindings(normalizedInput, slot, binding);
    } else if (BindingExists(normalizedInput, binding)) {
        return false;
    }

    if (slot == BindingSlot::Primary) {
        binding->primaryInputId = normalizedInput;
    } else {
        binding->secondaryInputId = normalizedInput;
    }
    return true;
}

bool ActionBindings::ClearBinding(std::string_view actionId, BindingSlot slot) noexcept {
    ActionBinding* binding = FindAction(actionId);
    if (binding == nullptr) {
        return false;
    }

    if (slot == BindingSlot::Primary) {
        binding->primaryInputId.clear();
    } else {
        binding->secondaryInputId.clear();
    }
    return true;
}

const ActionBinding* ActionBindings::FindAction(std::string_view actionId) const noexcept {
    const auto it = std::find_if(bindings_.begin(), bindings_.end(), [actionId](const ActionBinding& binding) {
        return binding.actionId == actionId;
    });
    return it == bindings_.end() ? nullptr : &(*it);
}

ActionBinding* ActionBindings::FindAction(std::string_view actionId) noexcept {
    const auto it = std::find_if(bindings_.begin(), bindings_.end(), [actionId](const ActionBinding& binding) {
        return binding.actionId == actionId;
    });
    return it == bindings_.end() ? nullptr : &(*it);
}

std::optional<std::string> ActionBindings::ResolveAction(std::string_view inputId) const {
    const ActionBinding* binding = FindBoundAction(inputId);
    if (binding == nullptr) {
        return std::nullopt;
    }
    return binding->actionId;
}

const ActionBinding* ActionBindings::FindBoundAction(std::string_view inputId) const noexcept {
    const std::string normalizedInput = NormalizeInputId(inputId);
    if (normalizedInput.empty()) {
        return nullptr;
    }

    const auto it = std::find_if(bindings_.begin(), bindings_.end(), [&normalizedInput](const ActionBinding& binding) {
        return binding.primaryInputId == normalizedInput || binding.secondaryInputId == normalizedInput;
    });
    return it == bindings_.end() ? nullptr : &(*it);
}

std::vector<ActionBinding> ActionBindings::Snapshot() const {
    return bindings_;
}

std::string ActionBindings::FormatInputLabel(std::string_view inputId) {
    return FormatInputLabelFromInputId(inputId);
}

std::string ActionBindings::NormalizeInputId(std::string_view inputId) {
    return NormalizeKeyboardInputId(inputId);
}

bool ActionBindings::BindingExists(std::string_view inputId, const ActionBinding* ignore) const noexcept {
    const std::string normalizedInput = NormalizeInputId(inputId);
    if (normalizedInput.empty()) {
        return false;
    }

    return std::any_of(bindings_.begin(), bindings_.end(), [&](const ActionBinding& binding) {
        if (&binding == ignore) {
            return false;
        }
        return binding.primaryInputId == normalizedInput || binding.secondaryInputId == normalizedInput;
    });
}

void ActionBindings::ClearConflictingBindings(std::string_view inputId,
                                              BindingSlot claimedSlot,
                                              const ActionBinding* owner) noexcept {
    const std::string normalizedInput = NormalizeInputId(inputId);
    if (normalizedInput.empty()) {
        return;
    }

    for (ActionBinding& binding : bindings_) {
        if (&binding == owner) {
            continue;
        }

        if (binding.primaryInputId == normalizedInput) {
            if (claimedSlot == BindingSlot::Primary) {
                binding.primaryInputId = binding.secondaryInputId;
                binding.secondaryInputId.clear();
            } else {
                binding.primaryInputId.clear();
            }
        }

        if (binding.secondaryInputId == normalizedInput) {
            binding.secondaryInputId.clear();
        }
    }
}

} // namespace ri::core
