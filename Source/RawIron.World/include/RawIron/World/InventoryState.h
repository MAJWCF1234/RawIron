#pragma once

// Hotbar / backpack / off-hand loadout, stacking, logic-gate binding — Documentation/02 Engine/Inventory and Possession.md

#include "RawIron/Logic/LogicTypes.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::world {

enum class InventoryItemKind {
    Generic,
    Utility,
    Consumable,
    Key,
};

enum class InventoryEquipSlot {
    None,
    OffHand,
};

enum class InventorySlotArea {
    Hotbar,
    Backpack,
    OffHand,
};

enum class InventoryQuickUseKind {
    None,
    EquipOffHand,
    ConsumeHealth,
    PresentKey,
};

enum class InventoryPresentationMode {
    Disabled,
    HiddenDataOnly,
    Visible,
};

struct InventoryPolicy {
    InventoryPresentationMode presentation = InventoryPresentationMode::Visible;
    std::size_t hotbarSize = 5;
    std::size_t backpackSize = 20;
    bool allowOffHand = true;
};

struct InventoryItemDefinition {
    std::string id;
    std::string displayName;
    InventoryItemKind kind = InventoryItemKind::Generic;
    InventoryEquipSlot preferredEquipSlot = InventoryEquipSlot::None;
    bool unique = true;
    int healAmount = 0;
    /// Units carried in this slot or amount to add in \ref InventoryLoadout::AddItem (minimum 1 when stored).
    int stackCount = 1;
};

struct InventorySlotRef {
    InventorySlotArea area = InventorySlotArea::Hotbar;
    std::size_t index = 0;
};

struct InventoryQuickUseContext {
    int currentHealth = 100;
    int maxHealth = 100;
};

struct InventoryQuickUseEffect {
    InventoryQuickUseKind kind = InventoryQuickUseKind::None;
    std::string itemId;
    int amount = 0;
    bool consumesItem = false;
};

struct InventoryOperationResult {
    bool accepted = false;
    std::string reason;
    std::optional<InventorySlotRef> changedSlot;
    std::optional<InventoryQuickUseEffect> quickUse;
};

struct InventorySnapshot {
    InventoryPresentationMode presentation = InventoryPresentationMode::Visible;
    std::size_t hotbarSize = 5;
    std::size_t backpackSize = 20;
    bool allowOffHand = true;
    std::vector<std::string> hotbarIds;
    std::vector<std::string> backpackIds;
    std::string offHandId;
    /// Parallel to `hotbarIds` / `backpackIds`; omitted entries default to 1 for non-empty ids.
    std::vector<int> hotbarCounts;
    std::vector<int> backpackCounts;
    int offHandCount = 0;
    std::size_t selectedHotbar = 0;
};

using InventoryItemResolver = std::function<std::optional<InventoryItemDefinition>(std::string_view id)>;

struct FlashlightBatteryModel {
    double maxCharge01 = 1.0;
    double drainPerSecond = 0.06;
    double rechargePerSecond = 0.03;
    double minimumOperationalCharge01 = 0.01;
};

class FlashlightBatteryState {
public:
    explicit FlashlightBatteryState(FlashlightBatteryModel model = {});

    void Configure(FlashlightBatteryModel model);
    void SetFlashlightEnabled(bool enabled);
    void SetCharge01(double charge01);
    [[nodiscard]] double Charge01() const noexcept;
    [[nodiscard]] bool IsFlashlightEnabled() const noexcept;
    [[nodiscard]] bool IsBeamActive() const noexcept;
    void Tick(double deltaSeconds);
    void Reset();

private:
    FlashlightBatteryModel model_{};
    double charge01_ = 1.0;
    bool flashlightEnabled_ = false;
};

class InventoryLoadout {
public:
    explicit InventoryLoadout(const InventoryPolicy& policy = {});

    [[nodiscard]] std::size_t HotbarSize() const;
    [[nodiscard]] std::size_t BackpackSize() const;
    [[nodiscard]] std::size_t SelectedHotbarSlot() const;
    [[nodiscard]] const InventoryPolicy& Policy() const;
    [[nodiscard]] const std::vector<std::optional<InventoryItemDefinition>>& Hotbar() const;
    [[nodiscard]] const std::vector<std::optional<InventoryItemDefinition>>& Backpack() const;
    [[nodiscard]] const std::optional<InventoryItemDefinition>& OffHand() const;
    [[nodiscard]] bool IsEnabled() const;
    [[nodiscard]] bool StoresGameplayItemData() const;
    [[nodiscard]] bool IsUiVisible() const;
    [[nodiscard]] std::size_t OccupiedHotbarSlots() const;
    [[nodiscard]] std::size_t OccupiedBackpackSlots() const;
    /// Filled storage slots (hotbar + backpack + off-hand whether empty or not counts only if occupied).
    [[nodiscard]] std::size_t TotalStoredItems() const;
    /// Sum of all stack units across hotbar, backpack, and off-hand.
    [[nodiscard]] std::size_t TotalItemUnits() const;

    void Clear();
    void SetSelectedHotbarSlot(std::size_t index);
    void SetPolicy(const InventoryPolicy& policy);

    [[nodiscard]] bool ContainsItemId(std::string_view itemId) const;
    /// Total units of `itemId` summed across stacks.
    [[nodiscard]] std::size_t CountItemId(std::string_view itemId) const;
    [[nodiscard]] std::optional<InventoryItemDefinition> ItemAt(const InventorySlotRef& slot) const;

    /// Selected hotbar slot — “main hand” / primary interaction item for quick-use and world prompts.
    [[nodiscard]] InventorySlotRef PrimaryHandSlot() const;
    [[nodiscard]] std::optional<InventoryItemDefinition> PrimaryHandItem() const;
    /// True if `itemId` is in the selected hotbar slot or the off-hand (player possession for interaction).
    [[nodiscard]] bool HoldsItemInHands(std::string_view itemId) const;

    /// Implements `LogicGraphSpec::inventoryQuery`-style checks: verifies quantity, optionally removes units
    /// (backpack → hotbar → off-hand). When `consume` is true, returns false if removal could not match the pre-check.
    [[nodiscard]] bool TryInventoryGate(std::string_view itemId, int quantity, bool consume);
    /// Builds an `InventoryQuery` for `LogicGraphSpec`. If `boundInstigatorId` is empty, any instigator matches.
    /// The returned closure captures `this`; keep the loadout alive at least as long as the graph uses the query.
    [[nodiscard]] ri::logic::InventoryQuery BindInventoryGateQuery(std::string_view boundInstigatorId);
    [[nodiscard]] InventoryOperationResult AddItem(const InventoryItemDefinition& item);
    [[nodiscard]] InventoryOperationResult QuickUseSlot(const InventorySlotRef& slot, const InventoryQuickUseContext& context);
    [[nodiscard]] InventoryOperationResult QuickUseSelectedHotbar(const InventoryQuickUseContext& context);
    [[nodiscard]] InventoryOperationResult StashHotbarSlotToBackpack(std::size_t hotbarIndex);
    [[nodiscard]] InventoryOperationResult MoveBackpackSlotToSelectedHotbar(std::size_t backpackIndex);
    [[nodiscard]] InventoryOperationResult SwapHands();

    [[nodiscard]] InventorySnapshot CaptureSnapshot() const;
    void RestoreSnapshot(const InventorySnapshot& snapshot, const InventoryItemResolver& resolver);

private:
    [[nodiscard]] std::optional<std::size_t> FindFirstEmptyHotbarSlot() const;
    [[nodiscard]] std::optional<std::size_t> FindFirstEmptyBackpackSlot() const;
    static void SanitizeStackInPlace(InventoryItemDefinition& item);
    void RemoveItemQuantity(std::string_view itemId, int quantity);
    [[nodiscard]] InventoryOperationResult MakeResult(bool accepted, std::string reason) const;

    InventoryPolicy policy_{};
    std::vector<std::optional<InventoryItemDefinition>> hotbar_;
    std::vector<std::optional<InventoryItemDefinition>> backpack_;
    std::optional<InventoryItemDefinition> offHand_;
    std::size_t selectedHotbar_ = 0;
};

} // namespace ri::world
