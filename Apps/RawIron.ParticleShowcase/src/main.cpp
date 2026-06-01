#include "RawIron/Audio/AudioBackendMiniaudio.h"
#include "RawIron/Audio/AudioManager.h"
#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Render/ShaderConfig.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Render/VulkanPreviewPresenter.h"
#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/ParticleSystem.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// winbase.h may #define GetCurrentTime to GetTickCount; breaks ri::audio::ManagedSound::GetCurrentTime.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#endif

namespace {

namespace fs = std::filesystem;

/// World-space layout: hero content sits slightly toward the back wall (–Z) for a clear backdrop and sight lines.
struct ShowcaseLayout {
    static constexpr ri::math::Vec3 kFocus{0.0f, 0.0f, -2.2f};
    static constexpr float kOrbitTargetY = 2.45f;
    static constexpr float kDefaultYaw = 124.0f;
    static constexpr float kDefaultPitch = -15.0f;
    static constexpr float kDefaultOrbitDistance = 15.25f;
    static constexpr float kEmberPillarY = 4.95f;
    static constexpr float kAmbientHazeY = 9.6f;
};

struct DecodedPcmMono {
    std::vector<float> mono;
    int sampleRate = 44100;
    bool valid = false;
};

bool DecodeWavFilePcm16Mono(const fs::path& path, DecodedPcmMono& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    char riff[12]{};
    in.read(riff, 12);
    if (in.gcount() != 12 || std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(riff + 8, "WAVE", 4) != 0) {
        return false;
    }
    std::uint16_t audioFormat = 0;
    std::uint16_t numChannels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    std::vector<std::uint8_t> dataChunk;

    while (in) {
        char chunkId[4]{};
        std::uint32_t chunkSize = 0;
        in.read(chunkId, 4);
        if (in.gcount() != 4) {
            break;
        }
        in.read(reinterpret_cast<char*>(&chunkSize), 4);
        if (!in) {
            break;
        }
        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            std::vector<std::uint8_t> fmt(chunkSize);
            in.read(reinterpret_cast<char*>(fmt.data()), chunkSize);
            if (static_cast<std::size_t>(in.gcount()) != chunkSize || chunkSize < 16) {
                return false;
            }
            std::memcpy(&audioFormat, fmt.data(), 2);
            std::memcpy(&numChannels, fmt.data() + 2, 2);
            std::memcpy(&sampleRate, fmt.data() + 4, 4);
            std::memcpy(&bitsPerSample, fmt.data() + 14, 2);
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            dataChunk.resize(chunkSize);
            in.read(reinterpret_cast<char*>(dataChunk.data()), chunkSize);
            break;
        } else {
            in.seekg(chunkSize, std::ios::cur);
        }
    }

    if (audioFormat != 1 || bitsPerSample != 16 || numChannels < 1 || numChannels > 2 || dataChunk.empty()) {
        return false;
    }

    const std::size_t bytesPerFrame = static_cast<std::size_t>(numChannels) * 2U;
    const std::size_t sampleFrames = dataChunk.size() / bytesPerFrame;
    out.mono.resize(sampleFrames);
    for (std::size_t i = 0; i < sampleFrames; ++i) {
        const std::uint8_t* frame = &dataChunk[i * bytesPerFrame];
        const auto sampleToFloat = [](const std::uint8_t* p) -> float {
            const std::int16_t v = static_cast<std::int16_t>(static_cast<std::uint16_t>(p[0] | (p[1] << 8)));
            return static_cast<float>(v) / 32768.0f;
        };
        float m = sampleToFloat(frame);
        if (numChannels == 2) {
            m = (m + sampleToFloat(frame + 2)) * 0.5f;
        }
        out.mono[i] = m;
    }
    out.sampleRate = static_cast<int>(sampleRate);
    out.valid = true;
    return true;
}

/// Short-window RMS — tracks kicks / drum hits better than a single long RMS.
float WindowRms(const DecodedPcmMono& pcm, double timeSec, int windowSamples) {
    if (!pcm.valid || pcm.mono.empty() || pcm.sampleRate <= 0) {
        return 0.0f;
    }
    const auto center = static_cast<std::int64_t>(timeSec * static_cast<double>(pcm.sampleRate));
    const std::int64_t half = static_cast<std::int64_t>(windowSamples / 2);
    const std::int64_t i0 = std::max<std::int64_t>(0, center - half);
    const std::int64_t i1 = std::min<std::int64_t>(static_cast<std::int64_t>(pcm.mono.size()), center + half);
    double acc = 0.0;
    for (std::int64_t i = i0; i < i1; ++i) {
        const double v = static_cast<double>(pcm.mono[static_cast<std::size_t>(i)]);
        acc += v * v;
    }
    const std::int64_t n = i1 - i0;
    return n > 0 ? static_cast<float>(std::sqrt(acc / static_cast<double>(n))) : 0.0f;
}

/// Max |Δsample| in a window — cheap onset / transient detector (snares, hats, vocal chops).
float WindowMaxAbsDelta(const DecodedPcmMono& pcm, double timeSec, int halfSpanSamples) {
    if (!pcm.valid || pcm.mono.empty() || pcm.sampleRate <= 0 || halfSpanSamples < 2) {
        return 0.0f;
    }
    const auto center = static_cast<std::int64_t>(timeSec * static_cast<double>(pcm.sampleRate));
    const std::int64_t i0 = std::max<std::int64_t>(1, center - static_cast<std::int64_t>(halfSpanSamples));
    const std::int64_t i1 =
        std::min<std::int64_t>(static_cast<std::int64_t>(pcm.mono.size()) - 1, center + static_cast<std::int64_t>(halfSpanSamples));
    float m = 0.0f;
    for (std::int64_t i = i0; i <= i1; ++i) {
        const float a = pcm.mono[static_cast<std::size_t>(i)];
        const float b = pcm.mono[static_cast<std::size_t>(i - 1)];
        m = std::max(m, std::abs(a - b));
    }
    return m;
}

fs::path DetectWorkspaceRoot(const fs::path& start) {
    fs::path current = fs::weakly_canonical(start);
    for (int guard = 0; guard < 40; ++guard) {
        std::error_code ec{};
        if (fs::exists(current / "CMakeLists.txt", ec) && fs::exists(current / "Assets", ec)
            && fs::exists(current / "Source", ec)) {
            return current;
        }
        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return fs::weakly_canonical(start);
}

void SetLightIntensity(ri::scene::Scene& scene, const int nodeHandle, const float intensity) {
    if (nodeHandle == ri::scene::kInvalidHandle) {
        return;
    }
    const int lightHandle = scene.GetNode(nodeHandle).light;
    if (lightHandle == ri::scene::kInvalidHandle) {
        return;
    }
    scene.GetLight(lightHandle).intensity = intensity;
}

void SetLightColor(ri::scene::Scene& scene, const int nodeHandle, const ri::math::Vec3& color) {
    if (nodeHandle == ri::scene::kInvalidHandle) {
        return;
    }
    const int lightHandle = scene.GetNode(nodeHandle).light;
    if (lightHandle == ri::scene::kInvalidHandle) {
        return;
    }
    scene.GetLight(lightHandle).color = color;
}

struct ShowcaseRuntime {
    ri::scene::Scene scene{"ParticleDevRoom"};
    ri::scene::OrbitCameraHandles orbit{};
    std::optional<ri::scene::CpuParticleSystem> particles;
    int particleBatch = ri::scene::kInvalidHandle;
    std::optional<ri::scene::CpuParticleSystem> particlesAmbient;
    int particleAmbientBatch = ri::scene::kInvalidHandle;
    std::optional<ri::scene::CpuParticleSystem> particlesKick;
    int particleKickBatch = ri::scene::kInvalidHandle;
    float kickBurstEnergy = 0.0f;
    /// Baseline configs for audio-driven modulation (rest frames match BuildDevRoom).
    ri::scene::CpuParticleSystemConfig primaryRest{};
    ri::scene::CpuParticleSystemConfig ambientRest{};
    int primaryMaterial = ri::scene::kInvalidHandle;
    int ambientMaterial = ri::scene::kInvalidHandle;
    int kickMaterial = ri::scene::kInvalidHandle;
    std::vector<int> pointLights;
    int keySunNode = ri::scene::kInvalidHandle;
    int spotWarm = ri::scene::kInvalidHandle;
    int spotCool = ri::scene::kInvalidHandle;
    DecodedPcmMono pcm{};
    /// Decode WAV analysis buffer on a side thread so Vulkan can open immediately (large tracks block for seconds).
    std::mutex pcmMutex;
    std::shared_ptr<ri::audio::AudioManager> audio;
    std::shared_ptr<ri::audio::ManagedSound> music;
    std::chrono::steady_clock::time_point lastTick{};
    float smoothRms = 0.0f;
    float peakTracker = 0.0f;
    float phaseHue = 0.0f;
    float kickSmooth = 0.0f;
    float snapSmooth = 0.0f;
    fs::path shaderCfgPath{};
    ri::render::ShaderPresentationConfig shaderCfg{};
    std::optional<fs::file_time_type> shaderCfgLastWrite{};
    double musicVolumeLinear = 0.82;
    ri::scene::OrbitCameraState orbitBaseline{};
    bool orbitFrozen = false;
};

void BuildDevRoom(ShowcaseRuntime& run) {
    const int root = run.scene.CreateNode("DevRoom");
    const ri::math::Vec3 P = ShowcaseLayout::kFocus;

    ri::scene::LightNodeOptions sun{};
    sun.nodeName = "KeySun";
    sun.parent = root;
    sun.transform.rotationDegrees = ri::math::Vec3{-56.0f, 36.0f, 0.0f};
    sun.light = ri::scene::Light{
        .name = "KeySun",
        .type = ri::scene::LightType::Directional,
        .color = ri::math::Vec3{1.0f, 0.95f, 0.88f},
        .intensity = 1.05f,
    };
    run.keySunNode = ri::scene::AddLightNode(run.scene, sun);

    auto addPoint = [&](const char* name, const ri::math::Vec3& pos, const ri::math::Vec3& color, float intensity) {
        ri::scene::LightNodeOptions L{};
        L.nodeName = name;
        L.parent = root;
        L.transform.position = pos;
        L.light = ri::scene::Light{
            .name = name,
            .type = ri::scene::LightType::Point,
            .color = color,
            .intensity = intensity,
            .range = 34.0f,
        };
        run.pointLights.push_back(ri::scene::AddLightNode(run.scene, L));
    };

    /// Keys pulled slightly toward the stage focus so the pillar reads against geometry.
    addPoint("FillMagenta", ri::math::Vec3{-11.4f, 7.35f, 2.2f}, ri::math::Vec3{0.95f, 0.35f, 0.95f}, 4.2f);
    addPoint("FillCyan", ri::math::Vec3{11.2f, 6.85f, -6.2f}, ri::math::Vec3{0.25f, 0.85f, 1.0f}, 4.0f);
    addPoint("RimGold", ri::math::Vec3{0.2f, 11.9f, -14.5f}, ri::math::Vec3{1.0f, 0.82f, 0.35f}, 3.4f);

    ri::scene::LightNodeOptions spotA{};
    spotA.nodeName = "SpotWarmStage";
    spotA.parent = root;
    spotA.transform.position = ri::math::Vec3{-7.6f, 12.6f, 7.2f};
    spotA.transform.rotationDegrees = ri::math::Vec3{-71.0f, -30.0f, 0.0f};
    spotA.light = ri::scene::Light{
        .name = "SpotWarmStage",
        .type = ri::scene::LightType::Spot,
        .color = ri::math::Vec3{1.0f, 0.72f, 0.38f},
        .intensity = 14.0f,
        .range = 50.0f,
        .spotAngleDegrees = 36.0f,
    };
    run.spotWarm = ri::scene::AddLightNode(run.scene, spotA);

    ri::scene::LightNodeOptions spotB{};
    spotB.nodeName = "SpotCoolStage";
    spotB.parent = root;
    spotB.transform.position = ri::math::Vec3{8.4f, 12.1f, 6.8f};
    spotB.transform.rotationDegrees = ri::math::Vec3{-69.5f, 27.5f, 0.0f};
    spotB.light = ri::scene::Light{
        .name = "SpotCoolStage",
        .type = ri::scene::LightType::Spot,
        .color = ri::math::Vec3{0.55f, 0.78f, 1.0f},
        .intensity = 12.0f,
        .range = 48.0f,
        .spotAngleDegrees = 33.0f,
    };
    run.spotCool = ri::scene::AddLightNode(run.scene, spotB);

    auto addWall = [&](const char* name, const ri::math::Vec3& pos, const ri::math::Vec3& scale,
                       const ri::math::Vec3& color, float metallic = 0.08f, float roughness = 0.42f) {
        ri::scene::PrimitiveNodeOptions wall{};
        wall.nodeName = name;
        wall.parent = root;
        wall.primitive = ri::scene::PrimitiveType::Cube;
        wall.materialName = std::string(name) + "Mat";
        wall.baseColor = color;
        wall.roughness = roughness;
        wall.metallic = metallic;
        wall.transform.position = pos;
        wall.transform.scale = scale;
        (void)ri::scene::AddPrimitiveNode(run.scene, wall);
    };

    addWall("Floor", ri::math::Vec3{0.0f, -0.06f, 0.0f}, ri::math::Vec3{52.0f, 0.35f, 40.0f},
            ri::math::Vec3{0.11f, 0.12f, 0.14f});
    addWall("WallNorth", ri::math::Vec3{0.0f, 8.6f, -19.9f}, ri::math::Vec3{52.0f, 18.5f, 0.38f},
            ri::math::Vec3{0.17f, 0.18f, 0.21f});
    addWall("WallSouth", ri::math::Vec3{0.0f, 8.6f, 19.9f}, ri::math::Vec3{52.0f, 18.5f, 0.38f},
            ri::math::Vec3{0.14f, 0.15f, 0.18f});
    addWall("WallWest", ri::math::Vec3{-25.1f, 8.6f, 0.0f}, ri::math::Vec3{0.38f, 18.5f, 40.0f},
            ri::math::Vec3{0.13f, 0.14f, 0.17f});
    addWall("WallEast", ri::math::Vec3{25.1f, 8.6f, 0.0f}, ri::math::Vec3{0.38f, 18.5f, 40.0f},
            ri::math::Vec3{0.14f, 0.15f, 0.18f});
    addWall("Ceiling", ri::math::Vec3{0.0f, 17.7f, 0.0f}, ri::math::Vec3{52.0f, 0.32f, 40.0f},
            ri::math::Vec3{0.09f, 0.10f, 0.12f});

    /// Dark “stage apron” under the hero — reads as a dedicated floor zone without changing outer bounds.
    addWall("StageApron", ri::math::Vec3{P.x, -0.03f, P.z + 0.15f}, ri::math::Vec3{20.0f, 0.12f, 15.0f},
            ri::math::Vec3{0.07f, 0.075f, 0.09f}, 0.12f, 0.55f);

    /// Horizontal band on the back wall to break up the large plane and anchor the composition.
    addWall("NorthAccentBand", ri::math::Vec3{P.x, 11.2f, -19.78f}, ri::math::Vec3{48.0f, 1.1f, 0.28f},
            ri::math::Vec3{0.22f, 0.23f, 0.27f}, 0.18f, 0.38f);

    /// Thin vertical posts frame the stage volume from the sides.
    addWall("StageFrameL", ri::math::Vec3{-10.2f, 5.4f, P.z - 0.4f}, ri::math::Vec3{0.55f, 9.2f, 0.55f},
            ri::math::Vec3{0.16f, 0.17f, 0.2f}, 0.15f, 0.4f);
    addWall("StageFrameR", ri::math::Vec3{10.2f, 5.4f, P.z - 0.4f}, ri::math::Vec3{0.55f, 9.2f, 0.55f},
            ri::math::Vec3{0.16f, 0.17f, 0.2f}, 0.15f, 0.4f);

    /// Tiered pedestal: wide pad + main block + thin cap (focus sits slightly back in the room).
    addWall("PedestalPad", ri::math::Vec3{P.x, 0.11f, P.z}, ri::math::Vec3{5.8f, 0.22f, 4.9f},
            ri::math::Vec3{0.14f, 0.15f, 0.17f}, 0.35f, 0.48f);
    addWall("PedestalCore", ri::math::Vec3{P.x, 0.62f, P.z}, ri::math::Vec3{3.0f, 0.72f, 3.0f},
            ri::math::Vec3{0.2f, 0.22f, 0.26f}, 0.72f, 0.34f);
    addWall("PedestalCap", ri::math::Vec3{P.x, 1.08f, P.z}, ri::math::Vec3{2.15f, 0.13f, 2.15f},
            ri::math::Vec3{0.28f, 0.3f, 0.34f}, 0.58f, 0.28f);

    /// Small reflective sphere as a focal anchor where the pillar emerges.
    {
        ri::scene::PrimitiveNodeOptions core{};
        core.nodeName = "EmitterCore";
        core.parent = root;
        core.primitive = ri::scene::PrimitiveType::Sphere;
        core.materialName = "EmitterCoreMat";
        core.baseColor = ri::math::Vec3{0.35f, 0.38f, 0.42f};
        core.emissiveColor = ri::math::Vec3{0.02f, 0.03f, 0.04f};
        core.metallic = 0.78f;
        core.roughness = 0.22f;
        core.transform.position = ri::math::Vec3{P.x, 1.34f, P.z};
        core.transform.scale = ri::math::Vec3{0.42f, 0.42f, 0.42f};
        (void)ri::scene::AddPrimitiveNode(run.scene, core);
    }

    ri::scene::OrbitCameraOptions cam{};
    cam.parent = root;
    cam.camera.fieldOfViewDegrees = 60.0f;
    cam.camera.nearClip = 0.08f;
    cam.camera.farClip = 220.0f;
    cam.orbit.target =
        ri::math::Vec3{ShowcaseLayout::kFocus.x, ShowcaseLayout::kOrbitTargetY, ShowcaseLayout::kFocus.z};
    cam.orbit.distance = ShowcaseLayout::kDefaultOrbitDistance;
    cam.orbit.yawDegrees = ShowcaseLayout::kDefaultYaw;
    cam.orbit.pitchDegrees = ShowcaseLayout::kDefaultPitch;
    run.orbit = ri::scene::AddOrbitCamera(run.scene, cam);
    run.orbitBaseline = ri::scene::OrbitCameraState{
        .target = cam.orbit.target,
        .distance = cam.orbit.distance,
        .yawDegrees = cam.orbit.yawDegrees,
        .pitchDegrees = cam.orbit.pitchDegrees,
    };

    ri::scene::CpuParticleSystemConfig cfg{};
    cfg.simulationMode = ri::scene::CpuParticleSimulationMode::FloatingAmbient;
    cfg.maxParticles = 1760;
    /// Tall narrow volume: swirling ember pillar above the pedestal (not an outward cone fountain).
    cfg.emitterCenter =
        ri::math::Vec3{ShowcaseLayout::kFocus.x, ShowcaseLayout::kEmberPillarY, ShowcaseLayout::kFocus.z};
    cfg.emitterRadius = 0.72f;
    cfg.emitterVolumeHalfExtents = ri::math::Vec3{0.5f, 6.1f, 0.5f};
    cfg.velocityMin = ri::math::Vec3{-2.0f, 0.85f, -2.0f};
    cfg.velocityMax = ri::math::Vec3{2.0f, 5.8f, 2.0f};
    cfg.particleLifeSeconds = 5.35f;
    cfg.gravityY = -0.52f;
    cfg.buoyancyAccelerationY = 2.05f;
    cfg.turbulenceAcceleration = 2.32f;
    cfg.turbulenceSpatialFrequency = 0.44f;
    cfg.turbulenceSecondaryScale = 2.95f;
    cfg.turbulenceSecondaryMix = 0.44f;
    cfg.homeSpringStrength = 0.24f;
    cfg.scaleMin = 0.04f;
    cfg.scaleMax = 0.32f;
    cfg.scaleLifetimeExponent = 0.38f;
    cfg.linearDragPerSecond = 0.44f;
    cfg.quadraticDragCoefficient = 0.0095f;
    cfg.respawnWhenBelowWorldY = -6.5f;
    cfg.windAcceleration = ri::math::Vec3{0.4f, 0.0f, -0.2f};
    cfg.spinAngularVelocityMin = ri::math::Vec3{-280.0f, -350.0f, -280.0f};
    cfg.spinAngularVelocityMax = ri::math::Vec3{280.0f, 350.0f, 280.0f};
    cfg.bouncePlaneWorldY = 0.115f;
    cfg.bounceRestitution = 0.44f;
    cfg.bounceXZVelocityScale = 0.78f;
    cfg.bounceCeilingWorldY = 16.1f;
    cfg.cameraFacingBillboards = true;
    cfg.seed = 0xDEADBEEFu;
    run.particles.emplace(cfg);

    const int mesh = run.scene.AddMesh(ri::scene::MakeBillboardQuadMesh("ShowcaseParticleBillboard"));
    run.primaryMaterial = run.scene.AddMaterial(ri::scene::Material{
        .name = "ShowcaseParticleMat",
        .shadingModel = ri::scene::ShadingModel::Unlit,
        .baseColor = ri::math::Vec3{0.85f, 0.45f, 0.14f},
        .emissiveColor = ri::math::Vec3{0.22f, 0.09f, 0.03f},
        .opacity = 0.32f,
        .additiveBlend = true,
    });

    run.particleBatch = run.scene.AddMeshInstanceBatch(ri::scene::MeshInstanceBatch{
        .name = "ShowcaseParticles",
        .parent = root,
        .mesh = mesh,
        .material = run.primaryMaterial,
        .transforms = {},
    });

    ri::scene::CpuParticleSystemConfig ambientCfg{};
    ambientCfg.simulationMode = ri::scene::CpuParticleSimulationMode::FloatingAmbient;
    ambientCfg.maxParticles = 1200;
    ambientCfg.emitterCenter =
        ri::math::Vec3{0.0f, ShowcaseLayout::kAmbientHazeY, -1.4f + ShowcaseLayout::kFocus.z * 0.35f};
    ambientCfg.emitterVolumeHalfExtents = ri::math::Vec3{18.5f, 5.6f, 15.0f};
    ambientCfg.bounceCeilingWorldY = 17.48f;
    ambientCfg.velocityMin = ri::math::Vec3{-0.32f, -0.26f, -0.32f};
    ambientCfg.velocityMax = ri::math::Vec3{0.32f, 0.38f, 0.32f};
    ambientCfg.particleLifeSeconds = 20.0f;
    ambientCfg.gravityY = -0.22f;
    ambientCfg.buoyancyAccelerationY = 1.05f;
    ambientCfg.turbulenceAcceleration = 4.2f;
    ambientCfg.turbulenceSpatialFrequency = 0.175f;
    ambientCfg.turbulenceSecondaryScale = 3.05f;
    ambientCfg.turbulenceSecondaryMix = 0.52f;
    ambientCfg.homeSpringStrength = 0.085f;
    ambientCfg.linearDragPerSecond = 0.38f;
    ambientCfg.quadraticDragCoefficient = 0.0068f;
    ambientCfg.windAcceleration = ri::math::Vec3{0.28f, 0.0f, -0.12f};
    ambientCfg.scaleMin = 0.032f;
    ambientCfg.scaleMax = 0.14f;
    ambientCfg.scaleLifetimeExponent = 0.38f;
    ambientCfg.spinAngularVelocityMin = ri::math::Vec3{-72.0f, -95.0f, -72.0f};
    ambientCfg.spinAngularVelocityMax = ri::math::Vec3{72.0f, 95.0f, 72.0f};
    ambientCfg.cameraFacingBillboards = true;
    ambientCfg.seed = 0xF10A7EEDu;
    run.particlesAmbient.emplace(ambientCfg);

    ri::scene::CpuParticleSystemConfig kickCfg{};
    kickCfg.simulationMode = ri::scene::CpuParticleSimulationMode::Fountain;
    kickCfg.maxParticles = 420;
    kickCfg.emitterCenter = ri::math::Vec3{ShowcaseLayout::kFocus.x, 1.34f, ShowcaseLayout::kFocus.z};
    kickCfg.emitterRadius = 0.38f;
    kickCfg.fountainConeHalfAngleDegrees = 42.0f;
    kickCfg.velocityMin = ri::math::Vec3{-1.4f, 7.5f, -1.4f};
    kickCfg.velocityMax = ri::math::Vec3{1.4f, 14.0f, 1.4f};
    kickCfg.particleLifeSeconds = 0.92f;
    kickCfg.gravityY = -13.5f;
    kickCfg.scaleMin = 0.05f;
    kickCfg.scaleMax = 0.24f;
    kickCfg.scaleLifetimeExponent = 0.72f;
    kickCfg.linearDragPerSecond = 0.18f;
    kickCfg.fountainTurbulenceAcceleration = 5.2f;
    kickCfg.turbulenceSpatialFrequency = 0.62f;
    kickCfg.bouncePlaneWorldY = 0.115f;
    kickCfg.bounceRestitution = 0.28f;
    kickCfg.cameraFacingBillboards = true;
    kickCfg.seed = 0xB00C5EEDu;
    run.particlesKick.emplace(kickCfg);

    run.ambientMaterial = run.scene.AddMaterial(ri::scene::Material{
        .name = "ShowcaseAmbientParticleMat",
        .shadingModel = ri::scene::ShadingModel::Unlit,
        .baseColor = ri::math::Vec3{0.35f, 0.65f, 0.85f},
        .emissiveColor = ri::math::Vec3{0.08f, 0.18f, 0.28f},
        .opacity = 0.22f,
        .additiveBlend = true,
    });
    run.particleAmbientBatch = run.scene.AddMeshInstanceBatch(ri::scene::MeshInstanceBatch{
        .name = "ShowcaseAmbientParticles",
        .parent = root,
        .mesh = mesh,
        .material = run.ambientMaterial,
        .transforms = {},
    });

    run.kickMaterial = run.scene.AddMaterial(ri::scene::Material{
        .name = "ShowcaseKickParticleMat",
        .shadingModel = ri::scene::ShadingModel::Unlit,
        .baseColor = ri::math::Vec3{1.0f, 0.92f, 0.72f},
        .emissiveColor = ri::math::Vec3{0.35f, 0.22f, 0.08f},
        .opacity = 0.48f,
        .additiveBlend = true,
    });
    run.particleKickBatch = run.scene.AddMeshInstanceBatch(ri::scene::MeshInstanceBatch{
        .name = "ShowcaseKickParticles",
        .parent = root,
        .mesh = mesh,
        .material = run.kickMaterial,
        .transforms = {},
    });

    if (run.particles.has_value()) {
        run.primaryRest = run.particles->Config();
    }
    if (run.particlesAmbient.has_value()) {
        run.ambientRest = run.particlesAmbient->Config();
    }

    run.lastTick = std::chrono::steady_clock::now();
}

void TickShowcase(ShowcaseRuntime& run,
                  const float dt,
                  const double musicTimeSec,
                  const float rmsTight,
                  const float rmsWide,
                  const float transient,
                  const double wallSeconds,
                  const float kickShortRms,
                  const float onsetDelta,
                  const bool holdBeatPhase) {
    if (run.audio != nullptr) {
        run.audio->Tick(static_cast<double>(dt) * 1000.0);
    }

    const float wide = std::max(rmsWide, 1.0e-4f);
    const float punch = std::clamp(rmsTight / wide, 0.0f, 6.0f);
    run.smoothRms += (rmsTight - run.smoothRms) * std::clamp(dt * 10.0f, 0.0f, 1.0f);
    run.peakTracker = std::max(run.peakTracker * std::exp(-dt * 2.8f), transient);
    run.kickSmooth += (kickShortRms - run.kickSmooth) * std::clamp(dt * 34.0f, 0.0f, 1.0f);
    const float snapInst = std::clamp(onsetDelta * 9.0f, 0.0f, 1.35f);
    run.snapSmooth += (snapInst - run.snapSmooth) * std::clamp(dt * 26.0f, 0.0f, 1.0f);

    const float epic = std::clamp(
        run.smoothRms * 0.95f + run.kickSmooth * 1.05f + run.snapSmooth * 1.4f + run.peakTracker * 0.95f + punch * 0.55f,
        0.0f,
        3.8f);

    if (!holdBeatPhase) {
        run.phaseHue += dt * (0.55f + punch * 2.4f + run.smoothRms * 5.5f + run.snapSmooth * 3.2f);
    }

    auto hueToRgb = [](float h) -> ri::math::Vec3 {
        h = std::fmod(h, 6.28f);
        return ri::math::Vec3{
            0.5f + 0.5f * std::sin(h),
            0.5f + 0.5f * std::sin(h + 2.09f),
            0.5f + 0.5f * std::sin(h + 4.18f),
        };
    };

    const ri::math::Vec3 mood = hueToRgb(run.phaseHue);

    if (run.keySunNode != ri::scene::kInvalidHandle) {
        SetLightIntensity(run.scene,
                          run.keySunNode,
                          std::clamp(0.95f + run.smoothRms * 0.75f + run.snapSmooth * 1.35f, 0.72f, 2.6f));
        SetLightColor(run.scene,
                      run.keySunNode,
                      ri::math::Vec3{
                          0.92f + mood.x * 0.08f + run.snapSmooth * 0.06f,
                          0.88f + mood.y * 0.1f,
                          0.78f + mood.z * 0.12f,
                      });
    }

    if (run.pointLights.size() >= 3) {
        SetLightColor(run.scene, run.pointLights[0],
                      ri::math::Vec3{
                          mood.x * 0.55f + 0.45f,
                          mood.y * 0.35f + 0.2f,
                          mood.z * 0.85f + 0.1f,
                      });
        SetLightColor(run.scene, run.pointLights[1],
                      ri::math::Vec3{
                          mood.z * 0.4f + 0.2f,
                          mood.x * 0.75f + 0.15f,
                          mood.y * 0.9f + 0.1f,
                      });
        SetLightColor(run.scene, run.pointLights[2],
                      ri::math::Vec3{
                          mood.y * 0.5f + 0.45f,
                          mood.z * 0.45f + 0.35f,
                          mood.x * 0.25f + 0.12f,
                      });

        SetLightIntensity(
            run.scene,
            run.pointLights[0],
            std::clamp(3.2f + run.smoothRms * 9.0f + punch * 3.5f + run.snapSmooth * 5.5f, 2.2f, 26.0f));
        SetLightIntensity(
            run.scene,
            run.pointLights[1],
            std::clamp(2.8f + transient * 11.0f + run.peakTracker * 7.5f + run.kickSmooth * 6.0f, 2.2f, 30.0f));
        SetLightIntensity(
            run.scene,
            run.pointLights[2],
            std::clamp(2.5f + wide * 7.0f + run.snapSmooth * 5.0f, 2.0f, 20.0f));
    }

    SetLightIntensity(
        run.scene,
        run.spotWarm,
        std::clamp(9.5f + punch * 16.0f + run.peakTracker * 11.0f + run.snapSmooth * 12.0f, 8.0f, 48.0f));
    SetLightIntensity(
        run.scene,
        run.spotCool,
        std::clamp(8.5f + run.smoothRms * 12.0f + run.kickSmooth * 9.0f, 7.5f, 40.0f));

    if (run.spotWarm != ri::scene::kInvalidHandle) {
        auto& n = run.scene.GetNode(run.spotWarm);
        n.localTransform.rotationDegrees.y =
            -30.0f + static_cast<float>(std::sin(wallSeconds * 1.7 + punch + run.snapSmooth * 3.0f)) * 14.0f;
        n.localTransform.rotationDegrees.x =
            -71.0f + static_cast<float>(std::sin(wallSeconds * 2.1)) * 8.0f;
    }
    if (run.spotCool != ri::scene::kInvalidHandle) {
        auto& n = run.scene.GetNode(run.spotCool);
        n.localTransform.rotationDegrees.y =
            27.5f + static_cast<float>(std::cos(wallSeconds * 1.5 + run.smoothRms * 6.0f)) * 14.0f;
    }

    if (run.particles.has_value()) {
        ri::scene::CpuParticleSystemConfig& c = run.particles->MutableConfig();
        const ri::scene::CpuParticleSystemConfig& b = run.primaryRest;
        const float pillarBoost = 1.0f + epic * 0.5f + transient * 0.36f;
        c.emitterVolumeHalfExtents = ri::math::Vec3{
            b.emitterVolumeHalfExtents.x * pillarBoost,
            b.emitterVolumeHalfExtents.y * (1.0f + epic * 0.11f + run.kickSmooth * 0.17f),
            b.emitterVolumeHalfExtents.z * pillarBoost,
        };
        c.velocityMax = ri::math::Vec3{
            b.velocityMax.x + epic * 1.25f + run.kickSmooth * 3.9f,
            b.velocityMax.y + transient * 15.0f + run.snapSmooth * 23.0f + run.kickSmooth * 10.0f,
            b.velocityMax.z + epic * 1.25f + run.kickSmooth * 3.9f,
        };
        c.velocityMin = ri::math::Vec3{
            std::max(-c.velocityMax.x * 0.95f, b.velocityMin.x - epic * 0.3f),
            std::min(b.velocityMin.y + epic * 2.5f + run.kickSmooth * 1.9f, c.velocityMax.y * 0.55f),
            std::max(-c.velocityMax.z * 0.95f, b.velocityMin.z - epic * 0.3f),
        };
        c.turbulenceAcceleration =
            b.turbulenceAcceleration * (1.0f + epic * 0.9f + run.snapSmooth * 1.02f);
        c.turbulenceSecondaryMix =
            std::clamp(b.turbulenceSecondaryMix + epic * 0.12f + run.snapSmooth * 0.18f, 0.0f, 1.0f);
        c.buoyancyAccelerationY =
            b.buoyancyAccelerationY * (1.0f + epic * 0.45f + run.kickSmooth * 0.52f + transient * 0.3f);
        c.homeSpringStrength = b.homeSpringStrength * (1.0f + epic * 0.38f + run.snapSmooth * 0.24f);
        c.windAcceleration = ri::math::Vec3{
            b.windAcceleration.x + run.snapSmooth * 0.32f,
            b.windAcceleration.y,
            b.windAcceleration.z - run.kickSmooth * 0.14f,
        };
    }

    if (run.particlesAmbient.has_value()) {
        ri::scene::CpuParticleSystemConfig& c = run.particlesAmbient->MutableConfig();
        const ri::scene::CpuParticleSystemConfig& b = run.ambientRest;
        const float vol = 1.0f + run.smoothRms * 0.18f + run.snapSmooth * 0.28f + run.kickSmooth * 0.12f;
        c.emitterVolumeHalfExtents = ri::math::Vec3{
            b.emitterVolumeHalfExtents.x * vol,
            b.emitterVolumeHalfExtents.y * (1.0f + run.smoothRms * 0.1f + epic * 0.06f),
            b.emitterVolumeHalfExtents.z * vol,
        };
        c.turbulenceAcceleration = b.turbulenceAcceleration * (1.0f + epic * 0.52f + run.snapSmooth * 0.72f);
        c.buoyancyAccelerationY = b.buoyancyAccelerationY * (1.0f + run.kickSmooth * 0.42f + transient * 0.35f);
    }

    if (run.primaryMaterial != ri::scene::kInvalidHandle) {
        ri::scene::Material& mat = run.scene.GetMaterial(run.primaryMaterial);
        const float strobe = run.snapSmooth * 0.55f + transient * 0.38f;
        mat.emissiveColor = ri::math::Vec3{
            std::clamp(0.18f + mood.x * 0.14f + strobe * 0.12f, 0.0f, 0.42f),
            std::clamp(0.08f + mood.y * 0.1f + strobe * 0.09f, 0.0f, 0.32f),
            std::clamp(0.03f + mood.z * 0.06f + strobe * 0.05f, 0.0f, 0.22f),
        };
        mat.opacity = std::clamp(0.22f + run.smoothRms * 0.28f + run.snapSmooth * 0.2f, 0.1f, 0.55f);
    }
    if (run.ambientMaterial != ri::scene::kInvalidHandle) {
        ri::scene::Material& mat = run.scene.GetMaterial(run.ambientMaterial);
        const float haze = run.smoothRms * 0.28f + epic * 0.16f;
        mat.emissiveColor = ri::math::Vec3{
            std::clamp(0.06f + mood.z * 0.08f + haze * 0.07f, 0.0f, 0.28f),
            std::clamp(0.12f + mood.x * 0.12f + haze * 0.12f, 0.0f, 0.35f),
            std::clamp(0.22f + mood.y * 0.1f + run.snapSmooth * 0.12f, 0.0f, 0.42f),
        };
        mat.opacity = std::clamp(0.18f + run.smoothRms * 0.22f + run.snapSmooth * 0.16f, 0.08f, 0.45f);
    }

    const ri::scene::OrbitCameraState& base = run.orbitBaseline;
    ri::scene::OrbitCameraState oc = run.orbit.orbit;
    if (!run.orbitFrozen) {
        const float t = static_cast<float>(wallSeconds);
        const float m = static_cast<float>(musicTimeSec);
        oc.yawDegrees = base.yawDegrees + std::sin(t * 0.31f) * 48.0f + std::sin(m * 0.078f) * 30.0f
            + std::sin(t * 1.65f + punch * 2.1f) * 14.0f + run.snapSmooth * 18.0f;
        oc.pitchDegrees = std::clamp(
            base.pitchDegrees + 3.0f + run.smoothRms * 13.0f + std::sin(t * 0.42f) * 8.0f + std::cos(m * 0.048f) * 6.0f
                - run.snapSmooth * 5.0f,
            base.pitchDegrees - 21.0f,
            base.pitchDegrees + 10.0f);
        oc.distance = std::clamp(
            base.distance - 1.0f + std::sin(t * 0.21f) * 3.0f + punch * 1.85f + run.smoothRms * 1.9f
                + std::sin(m * 0.031f) * 1.2f - epic * 0.85f,
            base.distance - 4.0f,
            base.distance + 5.0f);
    }

    if (run.orbit.camera != ri::scene::kInvalidHandle) {
        ri::scene::Camera& cam = run.scene.GetCamera(run.orbit.camera);
        cam.fieldOfViewDegrees =
            std::clamp(56.0f + epic * 10.0f + run.kickSmooth * 8.0f + run.snapSmooth * 10.0f, 48.0f, 82.0f);
    }

    ri::scene::SetOrbitCameraState(run.scene, run.orbit, oc);

    const ri::math::Vec3 cameraWorld = run.scene.ComputeWorldPosition(run.orbit.cameraNode);

    if (run.particles.has_value()) {
        run.particles->Step(dt);
        run.particles->ApplyInstanceTransforms(run.scene, run.particleBatch, cameraWorld);
    }
    if (run.particlesAmbient.has_value()) {
        run.particlesAmbient->Step(dt);
        run.particlesAmbient->ApplyInstanceTransforms(run.scene, run.particleAmbientBatch, cameraWorld);
    }
    if (run.particlesKick.has_value() && run.particleKickBatch != ri::scene::kInvalidHandle) {
        run.kickBurstEnergy = static_cast<float>(std::max(
            static_cast<double>(run.kickBurstEnergy) * std::exp(-static_cast<double>(dt) * 5.8),
            static_cast<double>(run.kickSmooth) * 1.35 + static_cast<double>(run.snapSmooth) * 1.75));
        const float burst = run.kickBurstEnergy;
        if (burst > 0.16f) {
            ri::scene::CpuParticleSystemConfig& kickLive = run.particlesKick->MutableConfig();
            kickLive.velocityMax = ri::math::Vec3{
                1.4f + burst * 0.8f,
                14.0f + burst * 26.0f + transient * 18.0f,
                1.4f + burst * 0.8f,
            };
            kickLive.fountainConeHalfAngleDegrees = 42.0f + burst * 18.0f;
            run.particlesKick->Step(dt);
            run.particlesKick->ApplyInstanceTransforms(run.scene, run.particleKickBatch, cameraWorld);
            if (run.kickMaterial != ri::scene::kInvalidHandle) {
                ri::scene::Material& kickMat = run.scene.GetMaterial(run.kickMaterial);
                kickMat.opacity = std::clamp(0.28f + burst * 0.42f, 0.18f, 0.72f);
                kickMat.emissiveColor = ri::math::Vec3{
                    std::clamp(0.28f + mood.x * 0.2f + burst * 0.18f, 0.0f, 0.75f),
                    std::clamp(0.16f + mood.y * 0.14f + burst * 0.12f, 0.0f, 0.55f),
                    std::clamp(0.06f + mood.z * 0.1f, 0.0f, 0.35f),
                };
            }
        } else {
            run.scene.GetMeshInstanceBatch(run.particleKickBatch).transforms.clear();
        }
    }
    ri::scene::MeshInstanceBatch& batch = run.scene.GetMeshInstanceBatch(run.particleBatch);
    const float pulse = 1.0f + run.smoothRms * 1.25f + punch * 0.45f + run.peakTracker * 0.55f + run.snapSmooth * 0.85f;
    for (ri::scene::Transform& tr : batch.transforms) {
        tr.scale = tr.scale * pulse;
    }
    if (run.particleAmbientBatch != ri::scene::kInvalidHandle) {
        ri::scene::MeshInstanceBatch& ambBatch = run.scene.GetMeshInstanceBatch(run.particleAmbientBatch);
        const float pulseAmb = 1.0f + run.smoothRms * 0.95f + punch * 0.35f + run.peakTracker * 0.42f + run.snapSmooth * 0.65f;
        for (ri::scene::Transform& tr : ambBatch.transforms) {
            tr.scale = tr.scale * pulseAmb;
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    const ri::core::CommandLine commandLine(argc, argv);
    if (commandLine.HasFlag("--help") || commandLine.HasFlag("-h")) {
        ri::core::LogInfo(
            "RawIron.ParticleShowcase — Vulkan native dev room: HDR + PBR, music-reactive fill/rim, soft billboard CPU particles.");
        ri::core::LogInfo("  --workspace=<path>   Repository root (auto-detected when omitted)");
        ri::core::LogInfo("  --audio=<path>       WAV file (PCM 16-bit). Default: <workspace>/Assets/audio/Porter Robinson - Polygon Dust.wav");
        ri::core::LogInfo("  --width / --height   Window size (default 1920x1080)");
        ri::core::LogInfo("  --silent             Visualizer motion without decoding WAV (time-based only)");
        ri::core::LogInfo("  Win32 while running: Esc quit | Space pause/resume | M mute | Up/Down volume | "
                          "O orbit lock | R reset camera | H controls help");
        ri::core::LogInfo(
            "  SPIR-V: set RAWIRON_VULKAN_SHADER_DIR to a folder containing *.spv if the app was moved off the "
            "build machine; otherwise shaders are resolved next to the exe or under Source/RawIron.Render.Vulkan/shaders.");
        ri::core::LogInfo(
            "  shader.cfg: Vulkan composite post stack (CAS, bloom, grading). Search: exe dir, then "
            "<workspace>/Assets/shader.cfg, <workspace>/shader.cfg, Content/shader.cfg.");
        ri::core::LogInfo(
            "  --no-shader-cfg      Skip shader.cfg (fixes blown-out / white image if the stack is too hot on your GPU).");
        ri::core::LogInfo(
            "  --hybrid-hdr         Enable multi-pass HDR + heavy composite (slower startup; can stall weak GPUs). "
            "Default is off for fast, stable loading.");
        ri::core::LogInfo(
            "  Reference HLSL packs resolve from Source/RawIron.Render.Vulkan/ReferenceShaders "
            "(legacy: vulkan/reference-shaders beside the exe); runtime tuning is shader.cfg.");
        return 0;
    }

    const fs::path workspace =
        commandLine.GetValue("--workspace").has_value()
            ? fs::weakly_canonical(fs::path(*commandLine.GetValue("--workspace")))
            : DetectWorkspaceRoot(fs::current_path());

    fs::path audioPath = workspace / "Assets" / "audio" / "Porter Robinson - Polygon Dust.wav";
    if (const auto a = commandLine.GetValue("--audio"); a.has_value() && !a->empty()) {
        const fs::path userPath(*a);
        audioPath = userPath.is_absolute() ? userPath : workspace / userPath;
    }

    const int width = std::clamp(commandLine.GetIntOr("--width", 1920), 480, 3840);
    const int height = std::clamp(commandLine.GetIntOr("--height", 1080), 270, 2160);
    const bool silent = commandLine.HasFlag("--silent");
    const bool noShaderCfg = commandLine.HasFlag("--no-shader-cfg");
    const bool hybridHdr = commandLine.HasFlag("--hybrid-hdr");

    auto runtime = std::make_shared<ShowcaseRuntime>();
    BuildDevRoom(*runtime);

    const fs::path exeDir = fs::weakly_canonical(fs::path(argv[0])).parent_path();
    if (!noShaderCfg) {
        if (const std::optional<fs::path> shaderResolved = ri::render::ResolveShaderCfgPath(workspace, exeDir)) {
            runtime->shaderCfgPath = *shaderResolved;
            std::string cfgErr;
            if (ri::render::LoadShaderCfg(*shaderResolved, &runtime->shaderCfg, &cfgErr)) {
                ri::core::LogInfo("Using shader.cfg (edit while running for hot-reload): "
                                   + shaderResolved->generic_string());
            } else if (!cfgErr.empty()) {
                ri::core::LogInfo(cfgErr);
                runtime->shaderCfg = {};
            }
            std::error_code wtEc;
            const auto initialWt = fs::last_write_time(*shaderResolved, wtEc);
            if (!wtEc) {
                runtime->shaderCfgLastWrite = initialWt;
            }
        }
    } else {
        ri::core::LogInfo("shader.cfg disabled (--no-shader-cfg); using music-reactive post values only.");
    }

    if (!silent && fs::exists(audioPath)) {
        const fs::path pcmPath = audioPath;
        std::weak_ptr<ShowcaseRuntime> pcmRuntime = runtime;
        std::thread([pcmPath, pcmRuntime]() {
            DecodedPcmMono local;
            if (!DecodeWavFilePcm16Mono(pcmPath, local)) {
                ri::core::LogInfo("Could not decode WAV for analysis; continuing with time-based motion only.");
                return;
            }
            if (const std::shared_ptr<ShowcaseRuntime> rt = pcmRuntime.lock()) {
                std::lock_guard<std::mutex> lock(rt->pcmMutex);
                rt->pcm = std::move(local);
            }
        }).detach();
    } else {
        runtime->pcm.valid = false;
        if (!silent) {
            ri::core::LogInfo("Audio file not found: " + audioPath.string());
        }
    }

    std::string audioErr;
    std::shared_ptr<ri::audio::AudioBackend> backend = ri::audio::CreateMiniaudioAudioBackend(&audioErr);
    if (backend != nullptr) {
        runtime->audio = std::make_shared<ri::audio::AudioManager>(backend);
        if (!silent && fs::exists(audioPath)) {
            runtime->music = runtime->audio->CreateManagedSound(audioPath.string(), runtime->musicVolumeLinear, false, 1.0);
            if (runtime->music != nullptr) {
                runtime->music->Play();
            }
        }
    } else if (!audioErr.empty()) {
        ri::core::LogInfo("Audio backend: " + audioErr);
    }

    const fs::path textureRoot = workspace / "Assets" / "Textures";
    const fs::path frameTextureRoot = fs::exists(textureRoot) ? textureRoot : fs::path{};
#if defined(_WIN32)
    HWND showcaseHwnd = nullptr;
#endif
    ri::render::vulkan::VulkanPreviewWindowOptions windowOptions{
        .windowTitle = hybridHdr ? "RawIron Particle Showcase — Vulkan Hybrid HDR" : "RawIron Particle Showcase — Vulkan",
        .textureRoot = frameTextureRoot,
#if defined(_WIN32)
        .outClientHwnd = &showcaseHwnd,
#endif
        // Hybrid HDR builds extra render passes + the heavy NativeComposite path; default off so the window and
        // first frames appear quickly and weak GPUs do not appear "frozen" during init.
        .enableHybridHdrPresentation = hybridHdr,
    };

    ri::core::LogInfo(std::string("ParticleShowcase: hybrid HDR composite ") + (hybridHdr ? "ON (--hybrid-hdr)"
                                                                                          : "OFF (default; faster startup)"));

    const auto wallStart = std::chrono::steady_clock::now();

    const ri::render::vulkan::VulkanNativeSceneFrameCallback buildFrame =
        [runtime, wallStart, frameTextureRoot
#if defined(_WIN32)
         ,
         &showcaseHwnd
#endif
    ](ri::render::vulkan::VulkanNativeSceneFrame& frame, std::string*) {
#if defined(_WIN32)
            if ((GetAsyncKeyState(VK_ESCAPE) & 0x0001) != 0) {
                PostQuitMessage(0);
            }
            if ((GetAsyncKeyState(VK_SPACE) & 0x0001) != 0 && runtime->music != nullptr) {
                if (runtime->music->IsPlaying()) {
                    runtime->music->Pause();
                } else {
                    runtime->music->Play();
                }
            }
            if ((GetAsyncKeyState('M') & 0x0001) != 0 && runtime->audio != nullptr) {
                runtime->audio->SetMuted(!runtime->audio->IsMuted());
            }
            if ((GetAsyncKeyState(VK_UP) & 0x0001) != 0 && runtime->music != nullptr) {
                runtime->musicVolumeLinear = std::min(1.0, runtime->musicVolumeLinear + 0.05);
                runtime->music->SetVolume(runtime->musicVolumeLinear);
            }
            if ((GetAsyncKeyState(VK_DOWN) & 0x0001) != 0 && runtime->music != nullptr) {
                runtime->musicVolumeLinear = std::max(0.0, runtime->musicVolumeLinear - 0.05);
                runtime->music->SetVolume(runtime->musicVolumeLinear);
            }
            if ((GetAsyncKeyState('O') & 0x0001) != 0) {
                runtime->orbitFrozen = !runtime->orbitFrozen;
            }
            if ((GetAsyncKeyState('R') & 0x0001) != 0) {
                runtime->orbitFrozen = false;
                ri::scene::SetOrbitCameraState(runtime->scene, runtime->orbit, runtime->orbitBaseline);
            }
            if ((GetAsyncKeyState('H') & 0x0001) != 0) {
                ri::core::LogInfo(
                    "ParticleShowcase keys: Esc quit | Space pause/resume music | M mute | Up/Down volume | O orbit "
                    "lock | R reset camera");
            }
#endif
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::clamp(
                std::chrono::duration<float>(now - runtime->lastTick).count(),
                1.0f / 240.0f,
                1.0f / 30.0f);
            runtime->lastTick = now;

            double musicTime = 0.0;
            if (runtime->music != nullptr) {
                // Use cursor position even while paused so analysis matches the frozen playback position.
                musicTime = runtime->music->GetCurrentTime();
            }

            const double wallSeconds = std::chrono::duration<double>(now - wallStart).count();

            float rmsTight = 0.12f;
            float rmsWide = 0.08f;
            float transient = 0.0f;
            float kickShort = 0.08f;
            float onsetDelta = 0.05f;
            bool usedPcmAnalysis = false;
            {
                std::lock_guard<std::mutex> lock(runtime->pcmMutex);
                if (runtime->pcm.valid) {
                    usedPcmAnalysis = true;
                    rmsTight = WindowRms(runtime->pcm, musicTime, 900);
                    rmsWide = WindowRms(runtime->pcm, musicTime, 6000);
                    transient = std::max(0.0f, rmsTight - rmsWide * 1.25f);
                    kickShort = WindowRms(runtime->pcm, musicTime, 420);
                    onsetDelta = WindowMaxAbsDelta(runtime->pcm, musicTime, 960);
                }
            }
            if (!usedPcmAnalysis) {
                const float t = static_cast<float>(wallSeconds);
                rmsTight = 0.08f + 0.06f * std::sin(t * 6.2f) + 0.04f * std::sin(t * 2.1f);
                rmsWide = 0.06f + 0.03f * std::sin(t * 0.8f);
                transient = std::max(0.0f, rmsTight - rmsWide);
                kickShort = 0.05f + 0.09f * std::sin(t * 11.0f) * std::sin(t * 0.23f);
                onsetDelta = 0.04f + 0.08f * std::abs(std::sin(t * 17.3f)) * std::abs(std::sin(t * 5.1f));
            }

            const bool holdBeatPhase = runtime->music != nullptr && !runtime->music->IsPlaying();
            TickShowcase(*runtime,
                         dt,
                         musicTime,
                         rmsTight,
                         rmsWide,
                         transient,
                         wallSeconds,
                         kickShort,
                         onsetDelta,
                         holdBeatPhase);

#if defined(_WIN32)
            if (showcaseHwnd != nullptr) {
                unsigned primaryCount = 0;
                unsigned ambientCount = 0;
                unsigned kickCount = 0;
                if (runtime->particleBatch != ri::scene::kInvalidHandle) {
                    primaryCount = static_cast<unsigned>(
                        runtime->scene.GetMeshInstanceBatch(runtime->particleBatch).transforms.size());
                }
                if (runtime->particleAmbientBatch != ri::scene::kInvalidHandle) {
                    ambientCount = static_cast<unsigned>(
                        runtime->scene.GetMeshInstanceBatch(runtime->particleAmbientBatch).transforms.size());
                }
                if (runtime->particleKickBatch != ri::scene::kInvalidHandle) {
                    kickCount = static_cast<unsigned>(
                        runtime->scene.GetMeshInstanceBatch(runtime->particleKickBatch).transforms.size());
                }
                const bool playing = runtime->music != nullptr && runtime->music->IsPlaying();
                const bool muted = runtime->audio != nullptr && runtime->audio->IsMuted();
                wchar_t title[384]{};
                swprintf_s(title,
                           L"Particle Showcase | rms=%.2f pk=%.2f | %u+%u+%u | vol %.0f%%%ls%ls%ls | Esc H",
                           static_cast<double>(runtime->smoothRms),
                           static_cast<double>(runtime->peakTracker),
                           primaryCount,
                           ambientCount,
                           kickCount,
                           static_cast<double>(runtime->musicVolumeLinear * 100.0),
                           playing ? L"" : L" pause",
                           muted ? L" mute" : L"",
                           runtime->orbitFrozen ? L" orbit-lock" : L"");
                SetWindowTextW(showcaseHwnd, title);
            }
#endif

            frame.scene = &runtime->scene;
            frame.cameraNode = runtime->orbit.cameraNode;
            frame.textureRoot = frameTextureRoot;
            frame.animationTimeSeconds =
                runtime->music != nullptr ? static_cast<float>(musicTime) : static_cast<float>(wallSeconds);
            frame.renderQualityTier = 2;
            // Scene-linear grade — keep modest; lights + unlit particles already add a lot of energy.
            frame.renderExposure = std::clamp(
                0.72f + runtime->smoothRms * 0.48f + runtime->peakTracker * 0.22f + runtime->snapSmooth * 0.32f,
                0.58f,
                1.12f);
            frame.renderContrast = 1.04f + runtime->smoothRms * 0.1f + runtime->snapSmooth * 0.12f;
            frame.renderSaturation = 1.06f + runtime->peakTracker * 0.22f + runtime->kickSmooth * 0.13f;
            frame.renderFogDensity = 0.0011f + runtime->smoothRms * 0.0058f - runtime->snapSmooth * 0.0007f;
            frame.postProcess.timeSeconds = static_cast<float>(wallSeconds);
            frame.postProcess.barrelDistortion =
                0.007f + runtime->smoothRms * 0.04f + transient * 0.04f + runtime->peakTracker * 0.022f
                + runtime->snapSmooth * 0.032f;
            frame.postProcess.noiseAmount = 0.0042f + runtime->peakTracker * 0.04f + runtime->smoothRms * 0.014f
                + runtime->snapSmooth * 0.027f;
            frame.postProcess.scanlineAmount = 0.0026f + runtime->smoothRms * 0.032f + transient * 0.022f;
            frame.postProcess.chromaticAberration = 0.001f + transient * 0.036f + runtime->snapSmooth * 0.024f;
            frame.postProcess.blurAmount = runtime->smoothRms * 0.0038f + transient * 0.01f + runtime->snapSmooth * 0.012f;
            frame.postProcess.tintStrength = 0.038f + runtime->smoothRms * 0.17f + runtime->snapSmooth * 0.14f;
            const float hue = runtime->phaseHue;
            frame.postProcess.tintColor = ri::math::Vec3{
                0.88f + 0.14f * std::sin(hue),
                0.90f + 0.12f * std::sin(hue + 2.1f),
                1.02f + 0.14f * std::sin(hue + 4.2f),
            };
            frame.postProcess.casSharpenAmount =
                0.26f + runtime->smoothRms * 0.2f + runtime->peakTracker * 0.1f + runtime->snapSmooth * 0.08f;
            frame.postProcess.casContrastAdaptation =
                0.2f + transient * 0.22f + runtime->kickSmooth * 0.14f;
            frame.postProcess.bloomIntensity =
                0.022f + runtime->peakTracker * 0.055f + runtime->smoothRms * 0.028f + transient * 0.038f;
            frame.postProcess.bloomThreshold =
                0.58f + runtime->kickSmooth * 0.28f + runtime->smoothRms * 0.12f;
            frame.postProcess.debandStrength =
                0.06f + runtime->smoothRms * 0.13f + transient * 0.1f + runtime->snapSmooth * 0.078f;

            if (!runtime->shaderCfgPath.empty() && runtime->shaderCfgLastWrite.has_value()) {
                std::error_code wtEc;
                const auto wt = fs::last_write_time(runtime->shaderCfgPath, wtEc);
                if (!wtEc && wt != *runtime->shaderCfgLastWrite) {
                    std::string relErr;
                    ri::render::ShaderPresentationConfig fresh{};
                    if (ri::render::LoadShaderCfg(runtime->shaderCfgPath, &fresh, &relErr)) {
                        runtime->shaderCfgLastWrite = wt;
                        runtime->shaderCfg = std::move(fresh);
                        ri::core::LogInfo("shader.cfg reloaded.");
                    } else if (!relErr.empty()) {
                        ri::core::LogInfo(relErr);
                    }
                }
            }
            ri::render::ApplyShaderConfig(frame.postProcess, runtime->shaderCfg);
            return true;
        };

    std::string error;
    const bool ok = ri::render::vulkan::RunVulkanNativeSceneLoop(width, height, buildFrame, windowOptions, &error);
    if (!ok && !error.empty()) {
        ri::core::LogInfo(error);
        return 1;
    }
    return 0;
}
