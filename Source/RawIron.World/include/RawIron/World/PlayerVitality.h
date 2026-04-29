#pragma once

namespace ri::world {

// `PlayerVitality` is an optional gameplay helper. Embed it only when a product needs health /
// invulnerability rules; the engine does not require it for movement, rendering, or content pipelines.

struct PlayerVitalityConfig {
    float maxHealth = 100.0f;
};

struct DamageOutcome {
    bool applied = false;
    bool died = false;
    bool blockedByInvulnerability = false;
    float healthAfter = 0.0f;
};

struct HealOutcome {
    bool healed = false;
    float healthAfter = 0.0f;
};

/// Authoritative health + timed invulnerability when you opt in to this module.
class PlayerVitality {
public:
    explicit PlayerVitality(PlayerVitalityConfig config = {});

    void SetConfig(PlayerVitalityConfig config);
    [[nodiscard]] PlayerVitalityConfig Config() const noexcept { return config_; }

    [[nodiscard]] float CurrentHealth() const noexcept { return health_; }
    [[nodiscard]] float MaxHealth() const noexcept { return config_.maxHealth; }
    [[nodiscard]] float HealthRatio() const noexcept;
    [[nodiscard]] bool IsAlive() const noexcept { return health_ > 0.0f; }

    void ResetToFullHealth() noexcept;

    /// Clamped assign for load / teleport sync.
    void SetHealth(float value) noexcept;

    [[nodiscard]] HealOutcome Heal(float amount) noexcept;

    /// `gameTimeSeconds` is monotonic game time (same clock as invulnerability end).
    [[nodiscard]] DamageOutcome ApplyDamage(double gameTimeSeconds, float amount) noexcept;

    void SetInvulnerableUntil(double gameTimeSeconds) noexcept;
    [[nodiscard]] double InvulnerableUntil() const noexcept { return invulnerableUntil_; }
    void ClearInvulnerability() noexcept { invulnerableUntil_ = 0.0; }
    [[nodiscard]] bool IsInvulnerable(double gameTimeSeconds) const noexcept;

private:
    static float SanitizeMaxHealth(float value) noexcept;
    static float ClampHealthToMax(float health, float maxHealth) noexcept;

    PlayerVitalityConfig config_;
    float health_ = 0.0f;
    double invulnerableUntil_ = 0.0;
};

} // namespace ri::world
