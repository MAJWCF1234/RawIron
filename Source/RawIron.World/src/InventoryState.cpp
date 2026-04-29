#include "RawIron/World/InventoryState.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <utility>

namespace ri::world {
namespace {

std::string NormalizeDisplayName(const InventoryItemDefinition& item) {
    return item.displayName.empty() ? item.id : item.displayName;
}

std::size_t CountOccupied(const std::vector<std::optional<InventoryItemDefinition>>& slots) {
    return static_cast<std::size_t>(std::count_if(slots.begin(), slots.end(), [](const auto& slot) {
        return slot.has_value();
    }));
}

int EffectiveStack(const InventoryItemDefinition& item) {
    return std::max(1, item.stackCount);
}

[[nodiscard]] int MergeStackTotals(int existingUnits, int addedUnits) {
    const auto sum = static_cast<std::int64_t>(existingUnits) + static_cast<std::int64_t>(addedUnits);
    if (sum >= INT_MAX) {
        return INT_MAX;
    }
    return static_cast<int>(sum);
}

} // namespace

FlashlightBatteryState::FlashlightBatteryState(FlashlightBatteryModel model) {
    Configure(model);
    Reset();
}

void FlashlightBatteryState::Configure(FlashlightBatteryModel model) {
    model.maxCharge01 = std::clamp(model.maxCharge01, 0.01, 1.0);
    model.drainPerSecond = std::max(0.0, model.drainPerSecond);
    model.rechargePerSecond = std::max(0.0, model.rechargePerSecond);
    model.minimumOperationalCharge01 = std::clamp(model.minimumOperationalCharge01, 0.0, model.maxCharge01);
    model_ = model;
    charge01_ = std::clamp(charge01_, 0.0, model_.maxCharge01);
}

void FlashlightBatteryState::SetFlashlightEnabled(bool enabled) {
    flashlightEnabled_ = enabled;
}

void FlashlightBatteryState::SetCharge01(double charge01) {
    charge01_ = std::clamp(charge01, 0.0, model_.maxCharge01);
}

double FlashlightBatteryState::Charge01() const noexcept {
    return charge01_;
}

bool FlashlightBatteryState::IsFlashlightEnabled() const noexcept {
    return flashlightEnabled_;
}

bool FlashlightBatteryState::IsBeamActive() const noexcept {
    return flashlightEnabled_ && charge01_ >= model_.minimumOperationalCharge01;
}

void FlashlightBatteryState::Tick(double deltaSeconds) {
    if (!(deltaSeconds > 0.0)) {
        return;
    }

    if (IsBeamActive()) {
        charge01_ = std::max(0.0, charge01_ - (model_.drainPerSecond * deltaSeconds));
    } else if (!flashlightEnabled_ && charge01_ < model_.maxCharge01) {
        charge01_ = std::min(model_.maxCharge01, charge01_ + (model_.rechargePerSecond * deltaSeconds));
    }
}

void FlashlightBatteryState::Reset() {
    charge01_ = model_.maxCharge01;
    flashlightEnabled_ = false;
}

void InventoryLoadout::SanitizeStackInPlace(InventoryItemDefinition& item) {
    item.stackCount = std::max(1, item.stackCount);
}

InventoryLoadout::InventoryLoadout(const InventoryPolicy& policy)
    : policy_(policy),
      hotbar_(std::max<std::size_t>(1U, policy.hotbarSize)),
      backpack_(std::max<std::size_t>(1U, policy.backpackSize)) {}

std::size_t InventoryLoadout::HotbarSize() const { return hotbar_.size(); }
std::size_t InventoryLoadout::BackpackSize() const { return backpack_.size(); }
std::size_t InventoryLoadout::SelectedHotbarSlot() const { return selectedHotbar_; }
const InventoryPolicy& InventoryLoadout::Policy() const { return policy_; }
const std::vector<std::optional<InventoryItemDefinition>>& InventoryLoadout::Hotbar() const { return hotbar_; }
const std::vector<std::optional<InventoryItemDefinition>>& InventoryLoadout::Backpack() const { return backpack_; }
const std::optional<InventoryItemDefinition>& InventoryLoadout::OffHand() const { return offHand_; }
bool InventoryLoadout::IsEnabled() const { return policy_.presentation != InventoryPresentationMode::Disabled; }
bool InventoryLoadout::StoresGameplayItemData() const { return policy_.presentation != InventoryPresentationMode::Disabled; }
bool InventoryLoadout::IsUiVisible() const { return policy_.presentation == InventoryPresentationMode::Visible; }
std::size_t InventoryLoadout::OccupiedHotbarSlots() const { return CountOccupied(hotbar_); }
std::size_t InventoryLoadout::OccupiedBackpackSlots() const { return CountOccupied(backpack_); }
std::size_t InventoryLoadout::TotalStoredItems() const {
    return OccupiedHotbarSlots() + OccupiedBackpackSlots() + (offHand_.has_value() ? 1U : 0U);
}

std::size_t InventoryLoadout::TotalItemUnits() const {
    std::size_t sum = 0U;
    const auto add = [&sum](const std::optional<InventoryItemDefinition>& slot) {
        if (slot.has_value()) {
            sum += static_cast<std::size_t>(EffectiveStack(*slot));
        }
    };
    for (const auto& slot : hotbar_) {
        add(slot);
    }
    for (const auto& slot : backpack_) {
        add(slot);
    }
    add(offHand_);
    return sum;
}

void InventoryLoadout::Clear() {
    for (auto& slot : hotbar_) {
        slot.reset();
    }
    for (auto& slot : backpack_) {
        slot.reset();
    }
    offHand_.reset();
    selectedHotbar_ = 0;
}

void InventoryLoadout::SetSelectedHotbarSlot(std::size_t index) {
    if (hotbar_.empty()) {
        selectedHotbar_ = 0;
        return;
    }
    selectedHotbar_ = std::min(index, hotbar_.size() - 1U);
}

void InventoryLoadout::SetPolicy(const InventoryPolicy& policy) {
    policy_ = policy;
    hotbar_.resize(std::max<std::size_t>(1U, policy_.hotbarSize));
    backpack_.resize(std::max<std::size_t>(1U, policy_.backpackSize));
    if (!policy_.allowOffHand) {
        offHand_.reset();
    }
    SetSelectedHotbarSlot(selectedHotbar_);
}

bool InventoryLoadout::ContainsItemId(std::string_view itemId) const {
    return CountItemId(itemId) > 0U;
}

std::size_t InventoryLoadout::CountItemId(std::string_view itemId) const {
    if (itemId.empty()) {
        return 0U;
    }
    std::size_t count = 0U;
    const auto addIf = [itemId, &count](const std::optional<InventoryItemDefinition>& slot) {
        if (slot.has_value() && slot->id == itemId) {
            count += static_cast<std::size_t>(EffectiveStack(*slot));
        }
    };
    addIf(offHand_);
    for (const auto& slot : hotbar_) {
        addIf(slot);
    }
    for (const auto& slot : backpack_) {
        addIf(slot);
    }
    return count;
}

std::optional<InventoryItemDefinition> InventoryLoadout::ItemAt(const InventorySlotRef& slot) const {
    switch (slot.area) {
    case InventorySlotArea::Hotbar:
        if (slot.index < hotbar_.size()) {
            return hotbar_[slot.index];
        }
        break;
    case InventorySlotArea::Backpack:
        if (slot.index < backpack_.size()) {
            return backpack_[slot.index];
        }
        break;
    case InventorySlotArea::OffHand:
        if (slot.index == 0U) {
            return offHand_;
        }
        break;
    }
    return std::nullopt;
}

InventorySlotRef InventoryLoadout::PrimaryHandSlot() const {
    return InventorySlotRef{.area = InventorySlotArea::Hotbar, .index = selectedHotbar_};
}

std::optional<InventoryItemDefinition> InventoryLoadout::PrimaryHandItem() const {
    return ItemAt(PrimaryHandSlot());
}

bool InventoryLoadout::HoldsItemInHands(std::string_view itemId) const {
    if (itemId.empty()) {
        return false;
    }
    const std::optional<InventoryItemDefinition> main = PrimaryHandItem();
    if (main.has_value() && main->id == itemId) {
        return true;
    }
    return offHand_.has_value() && offHand_->id == itemId;
}

void InventoryLoadout::RemoveItemQuantity(std::string_view itemId, int quantity) {
    if (itemId.empty() || quantity <= 0) {
        return;
    }
    int remaining = quantity;

    auto drainSlot = [&](std::optional<InventoryItemDefinition>& slot) {
        if (remaining <= 0 || !slot.has_value() || slot->id != itemId) {
            return;
        }
        const int stack = EffectiveStack(*slot);
        const int take = std::min(remaining, stack);
        remaining -= take;
        slot->stackCount = stack - take;
        if (slot->stackCount <= 0) {
            slot.reset();
        } else {
            SanitizeStackInPlace(*slot);
        }
    };

    for (auto& slot : backpack_) {
        drainSlot(slot);
    }
    for (auto& slot : hotbar_) {
        drainSlot(slot);
    }
    drainSlot(offHand_);
}

bool InventoryLoadout::TryInventoryGate(std::string_view itemId, int quantity, bool consume) {
    if (!StoresGameplayItemData() || itemId.empty() || quantity <= 0) {
        return false;
    }
    const std::size_t before = CountItemId(itemId);
    if (before < static_cast<std::size_t>(quantity)) {
        return false;
    }
    if (!consume) {
        return true;
    }
    RemoveItemQuantity(itemId, quantity);
    const std::size_t after = CountItemId(itemId);
    return after == before - static_cast<std::size_t>(quantity);
}

ri::logic::InventoryQuery InventoryLoadout::BindInventoryGateQuery(std::string_view boundInstigatorId) {
    return [this, bound = std::string(boundInstigatorId)](std::string_view instigatorId,
                                                          const std::string& itemId,
                                                          int quantity,
                                                          bool consume) -> bool {
        if (!bound.empty() && instigatorId != bound) {
            return false;
        }
        return TryInventoryGate(itemId, quantity, consume);
    };
}

InventoryOperationResult InventoryLoadout::AddItem(const InventoryItemDefinition& item) {
    if (!StoresGameplayItemData()) {
        return MakeResult(false, "inventory disabled");
    }
    if (item.id.empty()) {
        return MakeResult(false, "invalid item");
    }

    InventoryItemDefinition incoming = item;
    SanitizeStackInPlace(incoming);

    if (incoming.unique && ContainsItemId(incoming.id)) {
        return MakeResult(false, "already carried");
    }

    if (!incoming.unique) {
        if (policy_.allowOffHand && offHand_.has_value() && offHand_->id == incoming.id && !offHand_->unique) {
            offHand_->stackCount = MergeStackTotals(EffectiveStack(*offHand_), incoming.stackCount);
            InventoryOperationResult result = MakeResult(true, "stacked in off hand");
            result.changedSlot = InventorySlotRef{.area = InventorySlotArea::OffHand, .index = 0};
            return result;
        }
        for (std::size_t index = 0; index < hotbar_.size(); ++index) {
            if (hotbar_[index].has_value() && hotbar_[index]->id == incoming.id && !hotbar_[index]->unique) {
                hotbar_[index]->stackCount =
                    MergeStackTotals(EffectiveStack(*hotbar_[index]), incoming.stackCount);
                InventoryOperationResult result = MakeResult(true, "stacked on hotbar");
                result.changedSlot = InventorySlotRef{.area = InventorySlotArea::Hotbar, .index = index};
                return result;
            }
        }
        for (std::size_t index = 0; index < backpack_.size(); ++index) {
            if (backpack_[index].has_value() && backpack_[index]->id == incoming.id && !backpack_[index]->unique) {
                backpack_[index]->stackCount =
                    MergeStackTotals(EffectiveStack(*backpack_[index]), incoming.stackCount);
                InventoryOperationResult result = MakeResult(true, "stacked in backpack");
                result.changedSlot = InventorySlotRef{.area = InventorySlotArea::Backpack, .index = index};
                return result;
            }
        }
    }

    if (policy_.allowOffHand && incoming.preferredEquipSlot == InventoryEquipSlot::OffHand && !offHand_.has_value()) {
        offHand_ = incoming;
        InventoryOperationResult result = MakeResult(true, "equipped off hand");
        result.changedSlot = InventorySlotRef{.area = InventorySlotArea::OffHand, .index = 0};
        return result;
    }

    if (const std::optional<std::size_t> hotbarIndex = FindFirstEmptyHotbarSlot(); hotbarIndex.has_value()) {
        hotbar_[*hotbarIndex] = incoming;
        InventoryOperationResult result = MakeResult(true, "stored on hotbar");
        result.changedSlot = InventorySlotRef{.area = InventorySlotArea::Hotbar, .index = *hotbarIndex};
        return result;
    }

    if (const std::optional<std::size_t> backpackIndex = FindFirstEmptyBackpackSlot(); backpackIndex.has_value()) {
        backpack_[*backpackIndex] = incoming;
        InventoryOperationResult result = MakeResult(true, "stored in backpack");
        result.changedSlot = InventorySlotRef{.area = InventorySlotArea::Backpack, .index = *backpackIndex};
        return result;
    }

    return MakeResult(false, "inventory full");
}

InventoryOperationResult InventoryLoadout::QuickUseSlot(const InventorySlotRef& slot, const InventoryQuickUseContext& context) {
    if (!StoresGameplayItemData()) {
        return MakeResult(false, "inventory disabled");
    }
    if (slot.area == InventorySlotArea::OffHand) {
        return MakeResult(false, "off hand quick use unsupported");
    }

    std::optional<InventoryItemDefinition>* slotItem = nullptr;
    std::string emptyReason = "empty slot";
    if (slot.area == InventorySlotArea::Hotbar) {
        if (slot.index >= hotbar_.size()) {
            return MakeResult(false, "invalid hotbar slot");
        }
        slotItem = &hotbar_[slot.index];
        emptyReason = "empty hotbar slot";
    } else if (slot.area == InventorySlotArea::Backpack) {
        if (slot.index >= backpack_.size()) {
            return MakeResult(false, "invalid backpack slot");
        }
        slotItem = &backpack_[slot.index];
        emptyReason = "empty backpack slot";
    }
    if (slotItem == nullptr || !slotItem->has_value()) {
        return MakeResult(false, emptyReason);
    }

    const InventoryItemDefinition itemSnapshot = **slotItem;
    InventoryOperationResult result = MakeResult(true, "quick used " + NormalizeDisplayName(itemSnapshot));
    result.changedSlot = slot;

    if (policy_.allowOffHand && itemSnapshot.preferredEquipSlot == InventoryEquipSlot::OffHand) {
        std::optional<InventoryItemDefinition> previousOffHand = offHand_;
        offHand_ = itemSnapshot;
        *slotItem = previousOffHand;
        result.quickUse = InventoryQuickUseEffect{
            .kind = InventoryQuickUseKind::EquipOffHand,
            .itemId = itemSnapshot.id,
            .amount = 0,
            .consumesItem = false,
        };
        result.reason = "equipped " + NormalizeDisplayName(itemSnapshot) + " to off hand";
        return result;
    }

    if (itemSnapshot.kind == InventoryItemKind::Consumable && itemSnapshot.healAmount > 0) {
        if (context.currentHealth >= context.maxHealth) {
            return MakeResult(false, "health already full");
        }
        const int stack = EffectiveStack(itemSnapshot);
        if (stack <= 1) {
            slotItem->reset();
        } else {
            (*slotItem)->stackCount = stack - 1;
            SanitizeStackInPlace(**slotItem);
        }
        result.quickUse = InventoryQuickUseEffect{
            .kind = InventoryQuickUseKind::ConsumeHealth,
            .itemId = itemSnapshot.id,
            .amount = itemSnapshot.healAmount,
            .consumesItem = true,
        };
        result.reason = "consumed " + NormalizeDisplayName(itemSnapshot);
        return result;
    }

    if (itemSnapshot.kind == InventoryItemKind::Key) {
        result.quickUse = InventoryQuickUseEffect{
            .kind = InventoryQuickUseKind::PresentKey,
            .itemId = itemSnapshot.id,
            .amount = 0,
            .consumesItem = false,
        };
        result.reason = "presented key " + NormalizeDisplayName(itemSnapshot);
        return result;
    }

    return MakeResult(false, "item has no quick use");
}

InventoryOperationResult InventoryLoadout::QuickUseSelectedHotbar(const InventoryQuickUseContext& context) {
    return QuickUseSlot(InventorySlotRef{.area = InventorySlotArea::Hotbar, .index = selectedHotbar_}, context);
}

InventoryOperationResult InventoryLoadout::StashHotbarSlotToBackpack(std::size_t hotbarIndex) {
    if (!StoresGameplayItemData()) {
        return MakeResult(false, "inventory disabled");
    }
    if (hotbarIndex >= hotbar_.size() || !hotbar_[hotbarIndex].has_value()) {
        return MakeResult(false, "empty hotbar slot");
    }
    const std::optional<std::size_t> emptyBackpack = FindFirstEmptyBackpackSlot();
    if (!emptyBackpack.has_value()) {
        return MakeResult(false, "backpack full");
    }
    backpack_[*emptyBackpack] = hotbar_[hotbarIndex];
    hotbar_[hotbarIndex].reset();
    InventoryOperationResult result = MakeResult(true, "stashed to backpack");
    result.changedSlot = InventorySlotRef{.area = InventorySlotArea::Backpack, .index = *emptyBackpack};
    return result;
}

InventoryOperationResult InventoryLoadout::MoveBackpackSlotToSelectedHotbar(std::size_t backpackIndex) {
    if (!StoresGameplayItemData()) {
        return MakeResult(false, "inventory disabled");
    }
    if (backpackIndex >= backpack_.size() || !backpack_[backpackIndex].has_value()) {
        return MakeResult(false, "empty backpack slot");
    }
    if (selectedHotbar_ >= hotbar_.size()) {
        return MakeResult(false, "invalid selected hotbar slot");
    }
    std::swap(backpack_[backpackIndex], hotbar_[selectedHotbar_]);
    InventoryOperationResult result = MakeResult(true, "moved backpack item to hotbar");
    result.changedSlot = InventorySlotRef{.area = InventorySlotArea::Hotbar, .index = selectedHotbar_};
    return result;
}

InventoryOperationResult InventoryLoadout::SwapHands() {
    if (!StoresGameplayItemData()) {
        return MakeResult(false, "inventory disabled");
    }
    if (!policy_.allowOffHand) {
        return MakeResult(false, "off hand disabled");
    }
    if (selectedHotbar_ >= hotbar_.size()) {
        return MakeResult(false, "invalid selected hotbar slot");
    }
    std::swap(offHand_, hotbar_[selectedHotbar_]);
    InventoryOperationResult result = MakeResult(true, "swapped hands");
    result.changedSlot = InventorySlotRef{.area = InventorySlotArea::Hotbar, .index = selectedHotbar_};
    return result;
}

InventorySnapshot InventoryLoadout::CaptureSnapshot() const {
    InventorySnapshot snapshot{};
    snapshot.presentation = policy_.presentation;
    snapshot.hotbarSize = hotbar_.size();
    snapshot.backpackSize = backpack_.size();
    snapshot.allowOffHand = policy_.allowOffHand;
    snapshot.selectedHotbar = selectedHotbar_;
    snapshot.hotbarIds.reserve(hotbar_.size());
    snapshot.hotbarCounts.reserve(hotbar_.size());
    for (const auto& slot : hotbar_) {
        snapshot.hotbarIds.push_back(slot.has_value() ? slot->id : std::string{});
        snapshot.hotbarCounts.push_back(slot.has_value() ? EffectiveStack(*slot) : 0);
    }
    snapshot.backpackIds.reserve(backpack_.size());
    snapshot.backpackCounts.reserve(backpack_.size());
    for (const auto& slot : backpack_) {
        snapshot.backpackIds.push_back(slot.has_value() ? slot->id : std::string{});
        snapshot.backpackCounts.push_back(slot.has_value() ? EffectiveStack(*slot) : 0);
    }
    snapshot.offHandId = offHand_.has_value() ? offHand_->id : std::string{};
    snapshot.offHandCount = offHand_.has_value() ? EffectiveStack(*offHand_) : 0;
    return snapshot;
}

void InventoryLoadout::RestoreSnapshot(const InventorySnapshot& snapshot, const InventoryItemResolver& resolver) {
    Clear();
    policy_.presentation = snapshot.presentation;
    policy_.allowOffHand = snapshot.allowOffHand;
    hotbar_.resize(std::max<std::size_t>(1U, snapshot.hotbarSize));
    backpack_.resize(std::max<std::size_t>(1U, snapshot.backpackSize));
    policy_.hotbarSize = hotbar_.size();
    policy_.backpackSize = backpack_.size();
    if (!resolver) {
        return;
    }

    auto countAt = [](const std::vector<int>& counts, std::size_t index, bool hasItem) -> int {
        if (!hasItem) {
            return 0;
        }
        if (index < counts.size()) {
            const int c = counts[static_cast<int>(index)];
            return c > 0 ? c : 1;
        }
        return 1;
    };

    const std::size_t hotbarLimit = std::min(hotbar_.size(), snapshot.hotbarIds.size());
    for (std::size_t index = 0; index < hotbarLimit; ++index) {
        if (!snapshot.hotbarIds[index].empty()) {
            std::optional<InventoryItemDefinition> def = resolver(snapshot.hotbarIds[index]);
            if (def.has_value()) {
                def->stackCount = countAt(snapshot.hotbarCounts, index, true);
                SanitizeStackInPlace(*def);
                hotbar_[index] = std::move(def);
            }
        }
    }

    const std::size_t backpackLimit = std::min(backpack_.size(), snapshot.backpackIds.size());
    for (std::size_t index = 0; index < backpackLimit; ++index) {
        if (!snapshot.backpackIds[index].empty()) {
            std::optional<InventoryItemDefinition> def = resolver(snapshot.backpackIds[index]);
            if (def.has_value()) {
                def->stackCount = countAt(snapshot.backpackCounts, index, true);
                SanitizeStackInPlace(*def);
                backpack_[index] = std::move(def);
            }
        }
    }

    if (policy_.allowOffHand && !snapshot.offHandId.empty()) {
        offHand_ = resolver(snapshot.offHandId);
        if (offHand_.has_value()) {
            const int c = snapshot.offHandCount > 0 ? snapshot.offHandCount : 1;
            offHand_->stackCount = std::max(1, c);
        }
    }

    SetSelectedHotbarSlot(snapshot.selectedHotbar);
}

std::optional<std::size_t> InventoryLoadout::FindFirstEmptyHotbarSlot() const {
    for (std::size_t index = 0; index < hotbar_.size(); ++index) {
        if (!hotbar_[index].has_value()) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> InventoryLoadout::FindFirstEmptyBackpackSlot() const {
    for (std::size_t index = 0; index < backpack_.size(); ++index) {
        if (!backpack_[index].has_value()) {
            return index;
        }
    }
    return std::nullopt;
}

InventoryOperationResult InventoryLoadout::MakeResult(bool accepted, std::string reason) const {
    return InventoryOperationResult{
        .accepted = accepted,
        .reason = std::move(reason),
        .changedSlot = std::nullopt,
        .quickUse = std::nullopt,
    };
}

} // namespace ri::world
