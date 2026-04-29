#pragma once

#include <string_view>

namespace ri::logic::ports {

// --- Trigger volume (map layer) ----------------------------------------------------
inline constexpr std::string_view kTriggerOnStartTouch = "OnStartTouch";
inline constexpr std::string_view kTriggerOnEndTouch = "OnEndTouch";
inline constexpr std::string_view kTriggerOnStay = "OnStay";
inline constexpr std::string_view kTriggerEnable = "Enable";
inline constexpr std::string_view kTriggerDisable = "Disable";

// --- Spawner -----------------------------------------------------------------------
inline constexpr std::string_view kSpawnerOnSpawned = "OnSpawned";
inline constexpr std::string_view kSpawnerOnDespawned = "OnDespawned";
inline constexpr std::string_view kSpawnerOnFailed = "OnFailed";
inline constexpr std::string_view kSpawnerSpawn = "Spawn";
inline constexpr std::string_view kSpawnerDespawn = "Despawn";

// --- Door / mover ------------------------------------------------------------------
inline constexpr std::string_view kDoorOnOpened = "OnOpened";
inline constexpr std::string_view kDoorOnClosed = "OnClosed";
inline constexpr std::string_view kDoorOnLocked = "OnLocked";
inline constexpr std::string_view kDoorOpen = "Open";
inline constexpr std::string_view kDoorClose = "Close";
inline constexpr std::string_view kDoorLock = "Lock";
inline constexpr std::string_view kDoorUnlock = "Unlock";
inline constexpr std::string_view kDoorToggle = "Toggle";

// --- Keycard / interactable ---------------------------------------------------------
inline constexpr std::string_view kInteractOnInteract = "OnInteract";
inline constexpr std::string_view kInteractOnScan = "OnScan";

// --- Light / FX / audio -------------------------------------------------------------
inline constexpr std::string_view kFxEnable = "Enable";
inline constexpr std::string_view kFxDisable = "Disable";
inline constexpr std::string_view kFxSetIntensity = "SetIntensity";
inline constexpr std::string_view kFxPlay = "Play";

// --- Context fields (recommended keys for graph compatibility) ----------------------
inline constexpr std::string_view kFieldInstigatorKind = "instigatorKind";
inline constexpr std::string_view kFieldTags = "tags";
inline constexpr std::string_view kFieldFlags = "flags";

} // namespace ri::logic::ports
