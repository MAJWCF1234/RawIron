#include "RawIron/Scene/ParticleSystem.h"

#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Scene.h"

#include <algorithm>
#include <cmath>

namespace ri::scene {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;

float Rand01(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

float RandRange(std::uint32_t& state, float minValue, float maxValue) {
    return minValue + (maxValue - minValue) * Rand01(state);
}

ri::math::Vec3 RandomUnitDiskXY(std::uint32_t& state) {
    const float angle = RandRange(state, 0.0f, 6.28318530718f);
    const float r = std::sqrt(Rand01(state));
    return ri::math::Vec3{std::cos(angle) * r, 0.0f, std::sin(angle) * r};
}

/// Uniform random point inside a ball of given radius (center at origin).
ri::math::Vec3 RandomPointInBall(std::uint32_t& state, float radius) {
    const float u = Rand01(state);
    const float r = radius * std::cbrt(u);
    const float theta = RandRange(state, 0.0f, 6.28318530718f);
    const float z = RandRange(state, -1.0f, 1.0f);
    const float w = std::sqrt(std::max(0.0f, 1.0f - z * z));
    return ri::math::Vec3{std::cos(theta) * w * r, z * r, std::sin(theta) * w * r};
}

ri::math::Vec3 OrthonormalTangent(const ri::math::Vec3& axis) {
    if (std::abs(axis.x) < 0.9f) {
        return ri::math::Normalize(ri::math::Cross(axis, ri::math::Vec3{1.0f, 0.0f, 0.0f}));
    }
    return ri::math::Normalize(ri::math::Cross(axis, ri::math::Vec3{0.0f, 1.0f, 0.0f}));
}

/// Uniform direction within a right circular cone: axis (unit), polar half-angle [0, halfAngleRad].
ri::math::Vec3 RandomDirectionInCone(std::uint32_t& state, const ri::math::Vec3& axisUnit, float halfAngleRad) {
    const ri::math::Vec3 t = OrthonormalTangent(axisUnit);
    const ri::math::Vec3 b = ri::math::Normalize(ri::math::Cross(axisUnit, t));
    const float alpha = RandRange(state, 0.0f, std::max(0.0f, halfAngleRad));
    const float beta = RandRange(state, 0.0f, kPi * 2.0f);
    const float ca = std::cos(alpha);
    const float sa = std::sin(alpha);
    const float cb = std::cos(beta);
    const float sb = std::sin(beta);
    return ri::math::Normalize(axisUnit * ca + (t * cb + b * sb) * sa);
}

void ApplyQuadraticDrag(ri::math::Vec3& velocity, float k, float dt) {
    if (k <= 0.0f || dt <= 0.0f) {
        return;
    }
    const float speedSq = ri::math::LengthSquared(velocity);
    if (speedSq <= 1.0e-24f) {
        return;
    }
    const float speed = std::sqrt(speedSq);
    // Implicit Euler for dv/dt = -k |v| v  =>  v' = v / (1 + k |v| dt)
    velocity = velocity * (1.0f / (1.0f + k * speed * dt));
}

bool UsesEmitterVolumeBox(const CpuParticleSystemConfig& config) {
    return std::abs(config.emitterVolumeHalfExtents.x) > 1.0e-4f
        || std::abs(config.emitterVolumeHalfExtents.y) > 1.0e-4f
        || std::abs(config.emitterVolumeHalfExtents.z) > 1.0e-4f;
}

bool SpinConfigured(const CpuParticleSystemConfig& config) {
    return config.spinAngularVelocityMin.x != config.spinAngularVelocityMax.x
        || config.spinAngularVelocityMin.y != config.spinAngularVelocityMax.y
        || config.spinAngularVelocityMin.z != config.spinAngularVelocityMax.z;
}

ri::math::Vec3 TurbulenceOctave(const ri::math::Vec3& position,
                               float timeSeconds,
                               std::size_t particleIndex,
                               float spatialFrequency,
                               float amplitude) {
    if (amplitude <= 0.0f) {
        return ri::math::Vec3{};
    }
    const float f = std::max(spatialFrequency, 1.0e-4f);
    const float salt = static_cast<float>(particleIndex) * 0.713517f;
    const float px = position.x * f + salt;
    const float py = position.y * f * 0.87123f + salt * 0.313f;
    const float pz = position.z * f + timeSeconds * 0.712f;

    return ri::math::Vec3{
        std::sin(px + timeSeconds * 1.11f) * std::cos(pz * 0.803f + timeSeconds * 0.52f),
        std::sin(py + timeSeconds * 1.29f) * std::cos(px * 0.602f),
        std::cos(pz + timeSeconds * 0.91f) * std::sin(py * 0.551f + timeSeconds * 0.41f),
    } * amplitude;
}

/// Primary + optional finer octave (same phase style, scaled frequency).
void ApplyGroundBounceIfNeeded(ri::math::Vec3& position,
                               ri::math::Vec3& velocity,
                               const CpuParticleSystemConfig& config) {
    if (!config.bouncePlaneWorldY.has_value()) {
        return;
    }
    const float plane = *config.bouncePlaneWorldY;
    if (position.y >= plane) {
        return;
    }
    position.y = plane;
    if (velocity.y < 0.0f) {
        const float e = std::clamp(config.bounceRestitution, 0.0f, 1.35f);
        velocity.y = -velocity.y * e;
        const float xz = std::clamp(config.bounceXZVelocityScale, 0.0f, 1.5f);
        velocity.x *= xz;
        velocity.z *= xz;
    }
}

void ApplyCeilingBounceIfNeeded(ri::math::Vec3& position,
                                ri::math::Vec3& velocity,
                                const CpuParticleSystemConfig& config) {
    if (!config.bounceCeilingWorldY.has_value()) {
        return;
    }
    const float ceiling = *config.bounceCeilingWorldY;
    if (position.y <= ceiling) {
        return;
    }
    position.y = ceiling;
    if (velocity.y > 0.0f) {
        const float e = std::clamp(config.bounceRestitution, 0.0f, 1.35f);
        velocity.y = -velocity.y * e;
        const float xz = std::clamp(config.bounceXZVelocityScale, 0.0f, 1.5f);
        velocity.x *= xz;
        velocity.z *= xz;
    }
}

ri::math::Vec3 DualOctaveTurbulence(const ri::math::Vec3& position,
                                    float timeSeconds,
                                    std::size_t particleIndex,
                                    const CpuParticleSystemConfig& config,
                                    float baseStrength) {
    if (baseStrength <= 0.0f) {
        return ri::math::Vec3{};
    }
    ri::math::Vec3 acc =
        TurbulenceOctave(position, timeSeconds, particleIndex, config.turbulenceSpatialFrequency, baseStrength);
    const float mix = std::clamp(config.turbulenceSecondaryMix, 0.0f, 1.0f);
    if (mix > 0.0f) {
        const float fineFreq =
            config.turbulenceSpatialFrequency * std::max(config.turbulenceSecondaryScale, 0.25f);
        acc = acc + TurbulenceOctave(position, timeSeconds, particleIndex, fineFreq, baseStrength * mix);
    }
    return acc;
}

} // namespace

CpuParticleSystem::CpuParticleSystem(CpuParticleSystemConfig config)
    : config_(std::move(config)) {
    const std::size_t n = std::max<std::size_t>(1U, config_.maxParticles);
    particles_.resize(n);
    rng_ = config_.seed == 0u ? 0xA5A5A5A5u : config_.seed;
    simulationTimeSeconds_ = 0.0f;
    Reset();
}

void CpuParticleSystem::Reset() {
    rng_ = config_.seed == 0u ? 0xA5A5A5A5u : config_.seed;
    simulationTimeSeconds_ = 0.0f;
    for (std::size_t i = 0; i < particles_.size(); ++i) {
        Respawn(i);
        particles_[i].life = RandRange(rng_, 0.0f, particles_[i].maxLife);
    }
}

void CpuParticleSystem::Respawn(const std::size_t index) {
    Particle& p = particles_.at(index);
    if (config_.simulationMode == CpuParticleSimulationMode::FloatingAmbient) {
        if (UsesEmitterVolumeBox(config_)) {
            const ri::math::Vec3 h = config_.emitterVolumeHalfExtents;
            p.position = ri::math::Vec3{
                config_.emitterCenter.x + RandRange(rng_, -h.x, h.x),
                config_.emitterCenter.y + RandRange(rng_, -h.y, h.y),
                config_.emitterCenter.z + RandRange(rng_, -h.z, h.z),
            };
        } else {
            p.position = config_.emitterCenter + RandomPointInBall(rng_, config_.emitterRadius);
        }
    } else {
        const ri::math::Vec3 disk = RandomUnitDiskXY(rng_);
        p.position = config_.emitterCenter
            + ri::math::Vec3{disk.x * config_.emitterRadius, disk.y * config_.emitterRadius,
                             disk.z * config_.emitterRadius};
    }
    if (config_.simulationMode == CpuParticleSimulationMode::Fountain
        && config_.fountainConeHalfAngleDegrees > 1.0e-4f) {
        ri::math::Vec3 axis = ri::math::Normalize(config_.fountainEmitAxis);
        if (ri::math::LengthSquared(axis) < 1.0e-12f) {
            axis = ri::math::Vec3{0.0f, 1.0f, 0.0f};
        }
        const float halfRad = config_.fountainConeHalfAngleDegrees * kDegToRad;
        const ri::math::Vec3 dir = RandomDirectionInCone(rng_, axis, halfRad);
        const float vyMin = std::min(config_.velocityMin.y, config_.velocityMax.y);
        const float vyMax = std::max(config_.velocityMin.y, config_.velocityMax.y);
        const float speedLo = std::max(0.0f, vyMin);
        const float speedHi = std::max(speedLo + 1.0e-4f, vyMax);
        const float speed = RandRange(rng_, speedLo, speedHi);
        p.velocity = dir * speed;
    } else {
        p.velocity = ri::math::Vec3{
            RandRange(rng_, config_.velocityMin.x, config_.velocityMax.x),
            RandRange(rng_, config_.velocityMin.y, config_.velocityMax.y),
            RandRange(rng_, config_.velocityMin.z, config_.velocityMax.z),
        };
    }
    if (SpinConfigured(config_)) {
        p.angularVelocityDegreesPerSecond = ri::math::Vec3{
            RandRange(rng_, config_.spinAngularVelocityMin.x, config_.spinAngularVelocityMax.x),
            RandRange(rng_, config_.spinAngularVelocityMin.y, config_.spinAngularVelocityMax.y),
            RandRange(rng_, config_.spinAngularVelocityMin.z, config_.spinAngularVelocityMax.z),
        };
        p.rotationDegrees = ri::math::Vec3{
            RandRange(rng_, -180.0f, 180.0f),
            RandRange(rng_, -180.0f, 180.0f),
            RandRange(rng_, -180.0f, 180.0f),
        };
    } else {
        p.angularVelocityDegreesPerSecond = ri::math::Vec3{};
        p.rotationDegrees = ri::math::Vec3{};
    }
    p.maxLife = config_.particleLifeSeconds * RandRange(rng_, 0.88f, 1.12f);
    p.life = p.maxLife;
}

void CpuParticleSystem::StepFloatingAmbient(const float deltaSeconds) {
    const float gy = config_.gravityY;
    const float drag = config_.linearDragPerSecond;
    const float damp = drag > 0.0f ? std::exp(-drag * deltaSeconds) : 1.0f;
    const ri::math::Vec3 wind = config_.windAcceleration;
    const float buoy = config_.buoyancyAccelerationY;
    const float t = simulationTimeSeconds_;
    const float homeK = config_.homeSpringStrength;

    for (std::size_t i = 0; i < particles_.size(); ++i) {
        Particle& p = particles_.at(i);
        const ri::math::Vec3 turb =
            DualOctaveTurbulence(p.position, t, i, config_, config_.turbulenceAcceleration);
        p.velocity = p.velocity + turb * deltaSeconds;
        p.velocity.x += wind.x * deltaSeconds;
        p.velocity.y += (gy + wind.y + buoy) * deltaSeconds;
        p.velocity.z += wind.z * deltaSeconds;
        if (homeK != 0.0f) {
            const ri::math::Vec3 toCenter = config_.emitterCenter - p.position;
            p.velocity = p.velocity + toCenter * (homeK * deltaSeconds);
        }
        if (damp < 1.0f) {
            p.velocity = p.velocity * damp;
        }
        ApplyQuadraticDrag(p.velocity, config_.quadraticDragCoefficient, deltaSeconds);
        p.position = p.position + p.velocity * deltaSeconds;
        ApplyGroundBounceIfNeeded(p.position, p.velocity, config_);
        ApplyCeilingBounceIfNeeded(p.position, p.velocity, config_);

        if (config_.clampToEmitterVolume && UsesEmitterVolumeBox(config_)) {
            const ri::math::Vec3 h = config_.emitterVolumeHalfExtents;
            const ri::math::Vec3 local = p.position - config_.emitterCenter;
            if (std::abs(local.x) > h.x + 1.0e-3f || std::abs(local.y) > h.y + 1.0e-3f
                || std::abs(local.z) > h.z + 1.0e-3f) {
                Respawn(i);
                continue;
            }
        }

        if (config_.respawnBeyondRadius > 0.0f
            && ri::math::Distance(p.position, config_.emitterCenter) > config_.respawnBeyondRadius) {
            Respawn(i);
            continue;
        }
        if (config_.respawnWhenBelowWorldY.has_value()
            && p.position.y < *config_.respawnWhenBelowWorldY) {
            Respawn(i);
            continue;
        }
        p.rotationDegrees = p.rotationDegrees + p.angularVelocityDegreesPerSecond * deltaSeconds;
        p.life -= deltaSeconds;
        if (p.life <= 0.0f) {
            Respawn(i);
        }
    }
}

void CpuParticleSystem::StepFountain(const float deltaSeconds) {
    const float gy = config_.gravityY;
    const float drag = config_.linearDragPerSecond;
    const float damp = drag > 0.0f ? std::exp(-drag * deltaSeconds) : 1.0f;
    const ri::math::Vec3 wind = config_.windAcceleration;
    const float t = simulationTimeSeconds_;

    for (std::size_t i = 0; i < particles_.size(); ++i) {
        Particle& p = particles_[i];
        const ri::math::Vec3 turb =
            DualOctaveTurbulence(p.position, t, i, config_, config_.fountainTurbulenceAcceleration);
        p.velocity = p.velocity + turb * deltaSeconds;
        p.velocity.x += wind.x * deltaSeconds;
        p.velocity.y += (gy + wind.y) * deltaSeconds;
        p.velocity.z += wind.z * deltaSeconds;
        if (damp < 1.0f) {
            p.velocity = p.velocity * damp;
        }
        ApplyQuadraticDrag(p.velocity, config_.quadraticDragCoefficient, deltaSeconds);
        p.position = p.position + p.velocity * deltaSeconds;
        ApplyGroundBounceIfNeeded(p.position, p.velocity, config_);
        ApplyCeilingBounceIfNeeded(p.position, p.velocity, config_);
        if (config_.respawnWhenBelowWorldY.has_value() && p.position.y < *config_.respawnWhenBelowWorldY) {
            Respawn(i);
            continue;
        }
        p.rotationDegrees = p.rotationDegrees + p.angularVelocityDegreesPerSecond * deltaSeconds;
        p.life -= deltaSeconds;
        if (p.life <= 0.0f) {
            Respawn(i);
        }
    }
}

void CpuParticleSystem::Step(const float deltaSeconds) {
    if (deltaSeconds <= 0.0f) {
        return;
    }
    if (config_.simulationMode == CpuParticleSimulationMode::FloatingAmbient) {
        StepFloatingAmbient(deltaSeconds);
    } else {
        StepFountain(deltaSeconds);
    }
    simulationTimeSeconds_ += deltaSeconds;
}

void CpuParticleSystem::WriteInstanceTransforms(std::vector<Transform>& out) const {
    out.clear();
    out.reserve(particles_.size());
    for (const Particle& p : particles_) {
        const float t = p.maxLife > 1.0e-4f ? std::clamp(p.life / p.maxLife, 0.0f, 1.0f) : 0.0f;
        const float curveExp = std::clamp(config_.scaleLifetimeExponent, 0.03f, 8.0f);
        const float size =
            config_.scaleMin + (config_.scaleMax - config_.scaleMin) * std::pow(t, curveExp);
        Transform tr{};
        tr.position = p.position;
        tr.rotationDegrees = p.rotationDegrees;
        tr.scale = ri::math::Vec3{size, size, size};
        out.push_back(tr);
    }
}

void CpuParticleSystem::ApplyInstanceTransforms(Scene& scene, const int meshInstanceBatchHandle) const {
    std::vector<Transform> transforms;
    WriteInstanceTransforms(transforms);
    scene.GetMeshInstanceBatch(meshInstanceBatchHandle).transforms = std::move(transforms);
}

} // namespace ri::scene
