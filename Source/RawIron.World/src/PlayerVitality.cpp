#include "RawIron/World/PlayerVitality.h"

#include <algorithm>
#include <cmath>

namespace ri::world {

float PlayerVitality::SanitizeMaxHealth(float value) noexcept {
    if (!std::isfinite(value) || value <= 0.0f) {
        return 100.0f;
    }
    return value;
}

float PlayerVitality::ClampHealthToMax(float health, float maxHealth) noexcept {
    const float maxH = SanitizeMaxHealth(maxHealth);
    if (!std::isfinite(health)) {
        return maxH;
    }
    return std::max(0.0f, std::min(maxH, health));
}

PlayerVitality::PlayerVitality(PlayerVitalityConfig config) {
    SetConfig(config);
}

void PlayerVitality::SetConfig(PlayerVitalityConfig config) {
    config_.maxHealth = SanitizeMaxHealth(config.maxHealth);
    health_ = ClampHealthToMax(health_, config_.maxHealth);
}

float PlayerVitality::HealthRatio() const noexcept {
    const float maxH = config_.maxHealth;
    if (maxH <= 0.0f) {
        return 0.0f;
    }
    return std::max(0.0f, std::min(1.0f, health_ / maxH));
}

void PlayerVitality::ResetToFullHealth() noexcept {
    health_ = config_.maxHealth;
}

void PlayerVitality::SetHealth(float value) noexcept {
    health_ = ClampHealthToMax(value, config_.maxHealth);
}

HealOutcome PlayerVitality::Heal(float amount) noexcept {
    HealOutcome out{};
    out.healthAfter = health_;
    if (!IsAlive() || !std::isfinite(amount) || amount <= 0.0f) {
        return out;
    }
    const float maxH = config_.maxHealth;
    if (health_ >= maxH) {
        return out;
    }
    health_ = std::min(maxH, health_ + amount);
    out.healed = true;
    out.healthAfter = health_;
    return out;
}

DamageOutcome PlayerVitality::ApplyDamage(double gameTimeSeconds, float amount) noexcept {
    DamageOutcome out{};
    out.healthAfter = health_;
    if (IsInvulnerable(gameTimeSeconds)) {
        out.blockedByInvulnerability = std::isfinite(amount) && amount > 0.0f;
        return out;
    }
    if (!std::isfinite(amount) || amount <= 0.0f) {
        return out;
    }
    const float maxH = config_.maxHealth;
    if (!std::isfinite(health_)) {
        health_ = maxH;
    }
    health_ = ClampHealthToMax(health_ - amount, maxH);
    out.applied = true;
    out.died = !IsAlive();
    out.healthAfter = health_;
    return out;
}

void PlayerVitality::SetInvulnerableUntil(double gameTimeSeconds) noexcept {
    invulnerableUntil_ = gameTimeSeconds;
}

bool PlayerVitality::IsInvulnerable(double gameTimeSeconds) const noexcept {
    if (!std::isfinite(invulnerableUntil_) || !std::isfinite(gameTimeSeconds)) {
        return false;
    }
    return gameTimeSeconds < invulnerableUntil_;
}

} // namespace ri::world
