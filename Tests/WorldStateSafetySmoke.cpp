#include "RawIron/Runtime/RuntimeEventBus.h"
#include "RawIron/World/AccessFeedbackState.h"
#include "RawIron/World/DeveloperConsoleState.h"
#include "RawIron/World/DialogueCueState.h"
#include "RawIron/World/HudChannelTtlScheduler.h"
#include "RawIron/World/InventoryState.h"
#include "RawIron/World/PickupFeedbackState.h"
#include "RawIron/World/RuntimeState.h"
#include "RawIron/World/SignalBroadcastState.h"
#include "RawIron/World/TextOverlayEventBridge.h"
#include "RawIron/World/TextOverlayEvents.h"
#include "RawIron/World/TextOverlayState.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "check failed at line " << __LINE__ << ": " #condition "\n"; \
            return EXIT_FAILURE; \
        } \
    } while (false)

namespace {

bool HasErrorContaining(const ri::world::DeveloperConsoleState& console, const std::string_view token) {
    for (const auto& line : console.Scrollback()) {
        if (line.isError && line.text.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

ri::world::InventoryItemResolver InventoryResolver() {
    return [](const std::string_view id) -> std::optional<ri::world::InventoryItemDefinition> {
        if (id == "unique") {
            return ri::world::InventoryItemDefinition{.id = "unique", .unique = true};
        }
        if (id == "stack") {
            return ri::world::InventoryItemDefinition{.id = "stack", .unique = false};
        }
        if (id == "bad") {
            return ri::world::InventoryItemDefinition{.id = "", .unique = true};
        }
        return std::nullopt;
    };
}

} // namespace

int main() {
    using namespace ri::world;

    const double nan = std::numeric_limits<double>::quiet_NaN();
    FlashlightBatteryState battery({.maxCharge01 = nan,
                                    .drainPerSecond = nan,
                                    .rechargePerSecond = nan,
                                    .minimumOperationalCharge01 = nan});
    CHECK(std::isfinite(battery.Charge01()));
    CHECK(battery.Charge01() == 1.0);
    battery.SetCharge01(0.5);
    battery.SetCharge01(nan);
    CHECK(battery.Charge01() == 0.5);
    battery.Tick(std::numeric_limits<double>::infinity());
    CHECK(battery.Charge01() == 0.5);
    battery.Configure({.maxCharge01 = 1.0, .minimumOperationalCharge01 = 0.0});
    battery.SetCharge01(0.0);
    battery.SetFlashlightEnabled(true);
    CHECK(!battery.IsBeamActive());

    InventoryLoadout normalized({.hotbarSize = 0U, .backpackSize = 0U});
    CHECK(normalized.HotbarSize() == 1U && normalized.BackpackSize() == 1U);
    CHECK(normalized.Policy().hotbarSize == 1U && normalized.Policy().backpackSize == 1U);
    normalized.SetPolicy({.hotbarSize = std::numeric_limits<std::size_t>::max(),
                          .backpackSize = std::numeric_limits<std::size_t>::max()});
    CHECK(normalized.HotbarSize() == 4096U && normalized.BackpackSize() == 4096U);
    CHECK(normalized.Policy().hotbarSize == 4096U && normalized.Policy().backpackSize == 4096U);

    InventorySnapshot snapshot;
    snapshot.hotbarSize = std::numeric_limits<std::size_t>::max();
    snapshot.backpackSize = std::numeric_limits<std::size_t>::max();
    snapshot.hotbarIds = {"unique", "unique", "stack", "bad"};
    snapshot.hotbarCounts = {-5, 2, -10, 4};
    snapshot.backpackIds = {"unique"};
    snapshot.offHandId = "unique";
    normalized.RestoreSnapshot(snapshot, InventoryResolver());
    CHECK(normalized.HotbarSize() == 4096U && normalized.BackpackSize() == 4096U);
    CHECK(normalized.CountItemId("unique") == 1U);
    CHECK(normalized.CountItemId("stack") == 1U);
    CHECK(!normalized.ContainsItemId("bad"));
    CHECK(!normalized.OffHand().has_value());

    InventoryLoadout healing({.hotbarSize = 1U, .backpackSize = 1U});
    InventoryItemDefinition medkit{.id = "medkit",
                                   .displayName = "Medkit",
                                   .kind = InventoryItemKind::Consumable,
                                   .unique = false,
                                   .healAmount = 50,
                                   .stackCount = 2};
    CHECK(healing.AddItem(medkit).accepted);
    const auto healed = healing.QuickUseSelectedHotbar({.currentHealth = 95, .maxHealth = 100});
    CHECK(healed.accepted && healed.quickUse.has_value());
    CHECK(healed.quickUse->amount == 5);
    CHECK(healing.CountItemId("medkit") == 1U);
    const auto invalidHealth = healing.QuickUseSelectedHotbar({.currentHealth = -10, .maxHealth = 0});
    CHECK(!invalidHealth.accepted);
    CHECK(healing.CountItemId("medkit") == 1U);

    DeveloperConsoleState console(8U, 8U);
    console.SubmitCommand("alias loop loop");
    console.SubmitCommand("loop");
    CHECK(HasErrorContaining(console, "recursion limit"));
    console.RegisterScript("recursive", "exec recursive");
    console.SubmitCommand("exec recursive");
    CHECK(HasErrorContaining(console, "recursion limit"));
    const double sensitivityBefore = console.TuningValues().at("sensitivity");
    console.SubmitCommand("set sensitivity nan");
    CHECK(console.TuningValues().at("sensitivity") == sensitivityBefore);
    CHECK(HasErrorContaining(console, "Invalid numeric"));
    std::string importError = "stale";
    CHECK(!console.ImportTuningState("sensitivity=2.0&unknown=1", &importError));
    CHECK(console.TuningValues().at("sensitivity") == sensitivityBefore);
    CHECK(!importError.empty());
    CHECK(console.ImportTuningState("sensitivity=2.0", &importError));
    CHECK(importError.empty());
    CHECK(console.TuningValues().at("sensitivity") == 2.0);
    std::size_t customCalls = 0U;
    console.RegisterCommand("  custom  ", [&](std::string_view, std::string&, bool&) {
        ++customCalls;
        return true;
    });
    console.RegisterCommand("bad command", [&](std::string_view, std::string&, bool&) {
        customCalls += 100U;
        return true;
    });
    console.SubmitCommand("custom");
    console.SubmitCommand("bad command");
    CHECK(customCalls == 1U);
    CHECK(HasErrorContaining(console, "Unknown command"));
    DeveloperConsoleState chainConsole(8U, 2U);
    std::size_t chainCalls = 0U;
    chainConsole.RegisterCommand("x", [&](std::string_view, std::string&, bool&) {
        ++chainCalls;
        return true;
    });
    std::string chain;
    for (std::size_t index = 0; index < 256U; ++index) {
        chain += index == 0U ? "x" : ";x";
    }
    chainConsole.SubmitCommand(chain);
    CHECK(chainCalls == 256U);
    CHECK(!HasErrorContaining(chainConsole, "256 commands"));
    chainConsole.SubmitCommand(chain + ";x");
    CHECK(chainCalls == 512U);
    CHECK(HasErrorContaining(chainConsole, "256 commands"));
    DeveloperConsoleState shortConsole(2U, 2U);
    shortConsole.SubmitCommand(std::string(64U * 1024U + 1U, 'x'));
    CHECK(shortConsole.CommandHistory().empty());
    CHECK(HasErrorContaining(shortConsole, "too long"));

    TextOverlayState overlay;
    overlay.ShowMessage("visible", 1000.0);
    CHECK(overlay.Snapshot().messageBox.visible);
    overlay.ShowMessage("", 1000.0);
    CHECK(!overlay.Snapshot().messageBox.visible && overlay.DismissTimers().empty());
    overlay.ShowMessage("zero", 0.0);
    CHECK(!overlay.Snapshot().messageBox.visible && overlay.DismissTimers().empty());
    overlay.ShowSubtitle("fallback", nan);
    CHECK(overlay.Snapshot().subtitleLine.durationMs == 4000.0);

    HudChannelTtlScheduler hud;
    hud.Schedule("", "ignored", 1000.0);
    CHECK(!hud.Active("").has_value());
    hud.Schedule("notice", "hello", 1000.0);
    CHECK(hud.Active("notice").has_value());
    hud.Schedule("notice", "", 1000.0);
    CHECK(!hud.Active("notice").has_value());
    hud.Schedule("notice", "hello", 0.0);
    CHECK(!hud.Active("notice").has_value());

    ri::runtime::RuntimeEventBus eventBus;
    TextOverlayEventBridge bridge;
    bridge.Attach(eventBus, overlay);
    eventBus.Emit(text_overlay_events::kEventMessage,
                  {.fields = {{"text", "strict"}, {"durationMs", "12oops"}}});
    CHECK(overlay.Snapshot().messageBox.durationMs == 4000.0);
    eventBus.Emit(text_overlay_events::kEventLoadingProgress,
                  {.fields = {{"visible", " maybe "}, {"progress01", "0.5junk"}}});
    CHECK(overlay.Snapshot().blockers.loadingVisible);
    CHECK(overlay.Snapshot().blockers.loadingProgress01 == 0.0);
    eventBus.Emit(text_overlay_events::kEventLoadingProgress,
                  {.fields = {{"visible", " OFF "}, {"progress01", " 0.5 "}}});
    CHECK(!overlay.Snapshot().blockers.loadingVisible);
    CHECK(overlay.Snapshot().blockers.loadingProgress01 == 0.5);

    AccessFeedbackState access;
    AccessFeedbackRequest denied;
    denied.deniedDurationMs = 0.0;
    access.RecordDenied(denied);
    CHECK(!access.ActiveMessage().has_value());
    access.Advance(1000.0);
    access.RecordDenied(denied);
    access.Advance(1000.0);
    access.RecordDenied(denied);
    access.SetPolicy({.historyLimit = 1U});
    CHECK(access.History().size() == 1U);

    DialogueCueState dialogue;
    dialogue.Present({.dialogueText = "silent", .dialogueDurationMs = 0.0});
    CHECK(!dialogue.ActiveDialogue().has_value());
    dialogue.Present({.dialogueText = "one"});
    dialogue.Present({.dialogueText = "two"});
    dialogue.SetPolicy({.historyLimit = 1U});
    CHECK(dialogue.History().size() == 1U);

    PickupFeedbackState pickups({.antiSpamWindowMs = nan, .maxBurstsPerWindow = 1U});
    PickupFeedbackRequest pickup{.itemId = "item", .messageDurationMs = 0.0};
    for (int index = 0; index < 8; ++index) {
        pickups.RecordPickup(pickup);
        CHECK(!pickups.History().back().suppressedByAntiSpam);
    }
    CHECK(!pickups.ActiveMessage().has_value());
    pickups.SetPolicy({.antiSpamWindowMs = 0.0, .historyLimit = 1U});
    CHECK(pickups.History().size() == 1U);
    PickupFeedbackState burstPickups({.antiSpamWindowMs = 100.0, .maxBurstsPerWindow = 2U});
    burstPickups.RecordPickup({.itemId = "one"});
    burstPickups.RecordPickup({.itemId = "two"});
    for (int index = 0; index < 100; ++index) {
        burstPickups.RecordPickup({.itemId = "suppressed"});
        CHECK(burstPickups.History().back().suppressedByAntiSpam);
    }
    burstPickups.Advance(101.0);
    burstPickups.RecordPickup({.itemId = "fresh"});
    CHECK(!burstPickups.History().back().suppressedByAntiSpam);

    SignalBroadcastState signal;
    signal.Record({.message = "instant", .durationMs = 0.0});
    CHECK(!signal.ActiveMessage().has_value());
    CHECK(signal.History().size() == 1U && signal.History().front().message == "instant");
    signal.Record({.message = "two"});
    signal.SetPolicy({.historyLimit = 1U});
    CHECK(signal.History().size() == 1U && signal.History().front().message == "two");

    RuntimeEnvironmentService environment;
    SplineMeshDeformerPrimitive invalidMesh;
    invalidMesh.id = "invalid-mesh";
    invalidMesh.targetIds = {"target"};
    invalidMesh.splinePoints = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    invalidMesh.sampleCount = std::numeric_limits<std::uint32_t>::max();
    invalidMesh.maxSamples = 0U;
    environment.SetSplineMeshDeformerPrimitives({invalidMesh});
    const auto invalidMeshStates = environment.GetSplineMeshDeformerRuntimeStates({0.0f, 0.0f, 0.0f});
    CHECK(invalidMeshStates.size() == 1U);
    CHECK(invalidMeshStates.front().requestedSamples == 0U);
    CHECK(invalidMeshStates.front().generatedSegments == 0U);
    CHECK(!invalidMeshStates.front().active);

    SplineDecalRibbonPrimitive invalidRibbon;
    invalidRibbon.id = "invalid-ribbon";
    invalidRibbon.splinePoints = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    invalidRibbon.tessellation = std::numeric_limits<std::uint32_t>::max();
    invalidRibbon.maxSamples = 1U;
    environment.SetSplineDecalRibbonPrimitives({invalidRibbon});
    auto ribbonStates = environment.GetSplineDecalRibbonRuntimeStates({0.0f, 0.0f, 0.0f});
    CHECK(ribbonStates.size() == 1U);
    CHECK(ribbonStates.front().requestedSamples == 0U);
    CHECK(ribbonStates.front().generatedSegments == 0U);
    CHECK(ribbonStates.front().generatedTriangles == 0U);
    CHECK(!ribbonStates.front().active);

    invalidRibbon.maxSamples = std::numeric_limits<std::uint32_t>::max();
    environment.SetSplineDecalRibbonPrimitives({invalidRibbon});
    ribbonStates = environment.GetSplineDecalRibbonRuntimeStates({0.0f, 0.0f, 0.0f});
    CHECK(ribbonStates.front().requestedSamples == std::numeric_limits<std::uint32_t>::max());
    CHECK(ribbonStates.front().generatedSegments == std::numeric_limits<std::uint32_t>::max() - 1U);
    CHECK(ribbonStates.front().generatedTriangles == std::numeric_limits<std::uint32_t>::max());
    return EXIT_SUCCESS;
}
