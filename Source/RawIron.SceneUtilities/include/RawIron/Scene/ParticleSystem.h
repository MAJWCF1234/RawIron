#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Transform.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ri::scene {

class Scene;

enum class CpuParticleSimulationMode {
    /// Disk emitter on XZ, typical upward burst + gravity (default).
    Fountain = 0,
    /// Spawn inside a volume (box or sphere), low gravity, turbulence + optional buoyancy — dust motes / embers.
    FloatingAmbient = 1,
};

/// Lightweight CPU particle simulation for previews and tooling.
/// Uses a fixed pool; particles respawn at the emitter when their lifetime expires or bounds are hit.
struct CpuParticleSystemConfig {
    CpuParticleSimulationMode simulationMode = CpuParticleSimulationMode::Fountain;
    std::size_t maxParticles = 384;
    ri::math::Vec3 emitterCenter{};
    float emitterRadius = 0.18f;
    ri::math::Vec3 velocityMin{-0.55f, 2.4f, -0.55f};
    ri::math::Vec3 velocityMax{0.55f, 4.2f, 0.55f};
    float particleLifeSeconds = 2.4f;
    float gravityY = -9.2f;
    float scaleMin = 0.07f;
    float scaleMax = 0.20f;
    /// Size interpolates as `pow(life/maxLife, scaleLifetimeExponent)` toward `scaleMax` (default 0.5 ≈ previous sqrt).
    float scaleLifetimeExponent = 0.5f;
    std::uint32_t seed = 0xFACADEu;
    /// When > 0, velocity is scaled by exp(-linearDragPerSecond * dt) each step (air resistance).
    float linearDragPerSecond = 0.0f;
    /// Optional \f$|v|^2\f$ drag: acceleration \f$-k\,|v|\,\vec v\f$ (both modes); 0 disables.
    float quadraticDragCoefficient = 0.0f;
    /// When set, particles below this world Y are recycled like an out-of-bounds sink.
    std::optional<float> respawnWhenBelowWorldY{};
    /// Constant acceleration in world space (e.g. horizontal wind).
    ri::math::Vec3 windAcceleration{};
    /// When any component is > 0, FloatingAmbient spawns inside this axis-aligned box centered on `emitterCenter`.
    /// Otherwise FloatingAmbient uses a uniform sphere of radius `emitterRadius`.
    ri::math::Vec3 emitterVolumeHalfExtents{};
    /// Extra upward acceleration (FloatingAmbient); simulates heat lift / helium bias.
    float buoyancyAccelerationY = 0.0f;
    /// Strength of procedural acceleration field (FloatingAmbient).
    float turbulenceAcceleration = 0.0f;
    /// Spatial frequency for turbulence sampling (world units).
    float turbulenceSpatialFrequency = 0.35f;
    /// When > 0, FloatingAmbient respawns particles farther than this distance from `emitterCenter`.
    float respawnBeyondRadius = 0.0f;
    /// When true (default), FloatingAmbient respawns if `emitterVolumeHalfExtents` is used and the particle leaves that box.
    bool clampToEmitterVolume = true;
    /// Euler angular velocity bounds (deg/s) applied on respawn; (0,0,0) disables spin.
    ri::math::Vec3 spinAngularVelocityMin{};
    ri::math::Vec3 spinAngularVelocityMax{};
    /// Extra turbulence octave: scales spatial frequency of the fine layer (e.g. 1.8–3.2).
    float turbulenceSecondaryScale = 2.35f;
    /// Blend weight [0,1] for the secondary turbulence octave (default off).
    float turbulenceSecondaryMix = 0.0f;
    /// Acceleration `(emitterCenter - position) * strength` toward the emitter (FloatingAmbient / soft cohesion).
    float homeSpringStrength = 0.0f;
    /// Optional turbulence on Fountain mode (shares `turbulenceSpatialFrequency`); 0 disables.
    float fountainTurbulenceAcceleration = 0.0f;
    /// When set, particles hitting this world-Y plane from above bounce instead of tunneling (Fountain + FloatingAmbient).
    std::optional<float> bouncePlaneWorldY{};
    /// Optional ceiling Y; particles crossing upward through it bounce down (same restitution / XZ friction as floor).
    std::optional<float> bounceCeilingWorldY{};
    /// Vertical speed multiplier after bounce [0, ~1.2]; energy loss on impact.
    float bounceRestitution = 0.42f;
    /// Multiplier for horizontal velocity (XZ) on each bounce (simulates rough-floor friction).
    float bounceXZVelocityScale = 0.84f;
    /// When > 0, Fountain picks launch directions inside a cone around `fountainEmitAxis` (otherwise uses `velocityMin`/`velocityMax`).
    float fountainConeHalfAngleDegrees = 0.0f;
    /// Cone axis (normalized internally); default world +Y.
    ri::math::Vec3 fountainEmitAxis{0.0f, 1.0f, 0.0f};
};

class CpuParticleSystem {
public:
    explicit CpuParticleSystem(CpuParticleSystemConfig config);

    void Reset();
    void Step(float deltaSeconds);

    /// Writes one transform per live particle (world positions under the batch parent).
    void WriteInstanceTransforms(std::vector<Transform>& out) const;

    /// Copies current simulated transforms into an existing mesh-instance batch (replaces all instances).
    void ApplyInstanceTransforms(Scene& scene, int meshInstanceBatchHandle) const;

    [[nodiscard]] std::size_t ParticleCount() const noexcept { return particles_.size(); }
    [[nodiscard]] const CpuParticleSystemConfig& Config() const noexcept { return config_; }

private:
    struct Particle {
        ri::math::Vec3 position{};
        ri::math::Vec3 velocity{};
        ri::math::Vec3 rotationDegrees{};
        ri::math::Vec3 angularVelocityDegreesPerSecond{};
        float life = 0.0f;
        float maxLife = 1.0f;
    };

    void Respawn(std::size_t index);
    void StepFountain(float deltaSeconds);
    void StepFloatingAmbient(float deltaSeconds);

    CpuParticleSystemConfig config_;
    std::vector<Particle> particles_;
    std::uint32_t rng_;
    float simulationTimeSeconds_ = 0.0f;
};

} // namespace ri::scene
