#include "RawIron/Audio/AudioBackendMiniaudio.h"
#include "RawIron/Audio/AudioManager.h"
#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"
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
#include <optional>
#include <string>
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
    std::vector<int> pointLights;
    int spotWarm = ri::scene::kInvalidHandle;
    int spotCool = ri::scene::kInvalidHandle;
    DecodedPcmMono pcm{};
    std::shared_ptr<ri::audio::AudioManager> audio;
    std::shared_ptr<ri::audio::ManagedSound> music;
    std::chrono::steady_clock::time_point lastTick{};
    float smoothRms = 0.0f;
    float peakTracker = 0.0f;
    float phaseHue = 0.0f;
};

void BuildDevRoom(ShowcaseRuntime& run) {
    const int root = run.scene.CreateNode("DevRoom");

    ri::scene::LightNodeOptions sun{};
    sun.nodeName = "KeySun";
    sun.parent = root;
    sun.transform.rotationDegrees = ri::math::Vec3{-58.0f, 32.0f, 0.0f};
    sun.light = ri::scene::Light{
        .name = "KeySun",
        .type = ri::scene::LightType::Directional,
        .color = ri::math::Vec3{1.0f, 0.95f, 0.88f},
        .intensity = 2.4f,
    };
    (void)ri::scene::AddLightNode(run.scene, sun);

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
            .range = 32.0f,
        };
        run.pointLights.push_back(ri::scene::AddLightNode(run.scene, L));
    };

    addPoint("FillMagenta", ri::math::Vec3{-11.2f, 7.3f, 4.5f}, ri::math::Vec3{0.95f, 0.35f, 0.95f}, 11.0f);
    addPoint("FillCyan", ri::math::Vec3{11.0f, 6.9f, -3.9f}, ri::math::Vec3{0.25f, 0.85f, 1.0f}, 10.5f);
    addPoint("RimGold", ri::math::Vec3{0.0f, 11.8f, -12.9f}, ri::math::Vec3{1.0f, 0.82f, 0.35f}, 9.0f);

    ri::scene::LightNodeOptions spotA{};
    spotA.nodeName = "SpotWarmStage";
    spotA.parent = root;
    spotA.transform.position = ri::math::Vec3{-7.8f, 12.5f, 9.0f};
    spotA.transform.rotationDegrees = ri::math::Vec3{-72.0f, -28.0f, 0.0f};
    spotA.light = ri::scene::Light{
        .name = "SpotWarmStage",
        .type = ri::scene::LightType::Spot,
        .color = ri::math::Vec3{1.0f, 0.72f, 0.38f},
        .intensity = 48.0f,
        .range = 48.0f,
        .spotAngleDegrees = 38.0f,
    };
    run.spotWarm = ri::scene::AddLightNode(run.scene, spotA);

    ri::scene::LightNodeOptions spotB{};
    spotB.nodeName = "SpotCoolStage";
    spotB.parent = root;
    spotB.transform.position = ri::math::Vec3{8.2f, 12.0f, 8.6f};
    spotB.transform.rotationDegrees = ri::math::Vec3{-70.0f, 26.0f, 0.0f};
    spotB.light = ri::scene::Light{
        .name = "SpotCoolStage",
        .type = ri::scene::LightType::Spot,
        .color = ri::math::Vec3{0.55f, 0.78f, 1.0f},
        .intensity = 42.0f,
        .range = 46.0f,
        .spotAngleDegrees = 34.0f,
    };
    run.spotCool = ri::scene::AddLightNode(run.scene, spotB);

    auto addWall = [&](const char* name, const ri::math::Vec3& pos, const ri::math::Vec3& scale,
                       const ri::math::Vec3& color) {
        ri::scene::PrimitiveNodeOptions wall{};
        wall.nodeName = name;
        wall.parent = root;
        wall.primitive = ri::scene::PrimitiveType::Cube;
        wall.materialName = std::string(name) + "Mat";
        wall.baseColor = color;
        wall.roughness = 0.42f;
        wall.metallic = 0.08f;
        wall.transform.position = pos;
        wall.transform.scale = scale;
        (void)ri::scene::AddPrimitiveNode(run.scene, wall);
    };

    addWall("Floor", ri::math::Vec3{0.0f, -0.06f, 0.0f}, ri::math::Vec3{52.0f, 0.35f, 40.0f},
            ri::math::Vec3{0.12f, 0.13f, 0.15f});
    addWall("WallNorth", ri::math::Vec3{0.0f, 8.6f, -19.9f}, ri::math::Vec3{52.0f, 18.5f, 0.38f},
            ri::math::Vec3{0.18f, 0.19f, 0.22f});
    addWall("WallSouth", ri::math::Vec3{0.0f, 8.6f, 19.9f}, ri::math::Vec3{52.0f, 18.5f, 0.38f},
            ri::math::Vec3{0.16f, 0.17f, 0.20f});
    addWall("WallWest", ri::math::Vec3{-25.1f, 8.6f, 0.0f}, ri::math::Vec3{0.38f, 18.5f, 40.0f},
            ri::math::Vec3{0.14f, 0.15f, 0.18f});
    addWall("WallEast", ri::math::Vec3{25.1f, 8.6f, 0.0f}, ri::math::Vec3{0.38f, 18.5f, 40.0f},
            ri::math::Vec3{0.15f, 0.16f, 0.19f});
    addWall("Ceiling", ri::math::Vec3{0.0f, 17.7f, 0.0f}, ri::math::Vec3{52.0f, 0.32f, 40.0f},
            ri::math::Vec3{0.10f, 0.11f, 0.13f});

    ri::scene::PrimitiveNodeOptions pedestal{};
    pedestal.nodeName = "EmitterPedestal";
    pedestal.parent = root;
    pedestal.primitive = ri::scene::PrimitiveType::Cube;
    pedestal.materialName = "PedestalMat";
    pedestal.baseColor = ri::math::Vec3{0.22f, 0.24f, 0.28f};
    pedestal.metallic = 0.65f;
    pedestal.roughness = 0.35f;
    pedestal.transform.position = ri::math::Vec3{0.0f, 0.65f, 0.0f};
    pedestal.transform.scale = ri::math::Vec3{3.2f, 0.65f, 3.2f};
    (void)ri::scene::AddPrimitiveNode(run.scene, pedestal);

    ri::scene::OrbitCameraOptions cam{};
    cam.parent = root;
    cam.camera.fieldOfViewDegrees = 62.0f;
    cam.camera.nearClip = 0.08f;
    cam.camera.farClip = 220.0f;
    cam.orbit.target = ri::math::Vec3{0.0f, 2.8f, 0.0f};
    cam.orbit.distance = 14.5f;
    cam.orbit.yawDegrees = 132.0f;
    cam.orbit.pitchDegrees = -16.0f;
    run.orbit = ri::scene::AddOrbitCamera(run.scene, cam);

    ri::scene::CpuParticleSystemConfig cfg{};
    cfg.simulationMode = ri::scene::CpuParticleSimulationMode::Fountain;
    cfg.maxParticles = 720;
    cfg.emitterCenter = ri::math::Vec3{0.0f, 3.35f, 0.0f};
    cfg.emitterRadius = 0.55f;
    cfg.velocityMin = ri::math::Vec3{-1.1f, 3.2f, -1.1f};
    cfg.velocityMax = ri::math::Vec3{1.1f, 6.5f, 1.1f};
    cfg.fountainConeHalfAngleDegrees = 24.0f;
    cfg.fountainEmitAxis = ri::math::Vec3{0.12f, 1.0f, -0.05f};
    cfg.particleLifeSeconds = 3.1f;
    cfg.gravityY = -11.0f;
    cfg.scaleMin = 0.05f;
    cfg.scaleMax = 0.22f;
    cfg.scaleLifetimeExponent = 0.44f;
    cfg.linearDragPerSecond = 0.85f;
    cfg.quadraticDragCoefficient = 0.011f;
    cfg.respawnWhenBelowWorldY = -8.0f;
    cfg.windAcceleration = ri::math::Vec3{0.45f, 0.0f, -0.18f};
    cfg.fountainTurbulenceAcceleration = 0.72f;
    cfg.turbulenceSpatialFrequency = 0.42f;
    cfg.turbulenceSecondaryScale = 2.55f;
    cfg.turbulenceSecondaryMix = 0.26f;
    cfg.spinAngularVelocityMin = ri::math::Vec3{-260.0f, -320.0f, -260.0f};
    cfg.spinAngularVelocityMax = ri::math::Vec3{260.0f, 320.0f, 260.0f};
    cfg.bouncePlaneWorldY = 0.115f;
    cfg.bounceRestitution = 0.48f;
    cfg.bounceXZVelocityScale = 0.81f;
    cfg.seed = 0xDEADBEEFu;
    run.particles.emplace(cfg);

    const int mesh = run.scene.AddMesh(ri::scene::MakeUvSphereMesh("ShowcaseParticleMesh"));
    const int mat = run.scene.AddMaterial(ri::scene::Material{
        .name = "ShowcaseParticleMat",
        .shadingModel = ri::scene::ShadingModel::Unlit,
        .baseColor = ri::math::Vec3{0.92f, 0.55f, 0.22f},
        .emissiveColor = ri::math::Vec3{0.48f, 0.22f, 0.08f},
        .opacity = 0.42f,
        .additiveBlend = true,
    });

    run.particleBatch = run.scene.AddMeshInstanceBatch(ri::scene::MeshInstanceBatch{
        .name = "ShowcaseParticles",
        .parent = root,
        .mesh = mesh,
        .material = mat,
        .transforms = {},
    });

    ri::scene::CpuParticleSystemConfig ambientCfg{};
    ambientCfg.simulationMode = ri::scene::CpuParticleSimulationMode::FloatingAmbient;
    ambientCfg.maxParticles = 640;
    ambientCfg.emitterCenter = ri::math::Vec3{0.0f, 9.8f, 0.0f};
    ambientCfg.emitterVolumeHalfExtents = ri::math::Vec3{17.5f, 5.2f, 13.5f};
    ambientCfg.bounceCeilingWorldY = 17.48f;
    ambientCfg.velocityMin = ri::math::Vec3{-0.32f, -0.26f, -0.32f};
    ambientCfg.velocityMax = ri::math::Vec3{0.32f, 0.38f, 0.32f};
    ambientCfg.particleLifeSeconds = 20.0f;
    ambientCfg.gravityY = -0.22f;
    ambientCfg.buoyancyAccelerationY = 1.05f;
    ambientCfg.turbulenceAcceleration = 3.4f;
    ambientCfg.turbulenceSpatialFrequency = 0.165f;
    ambientCfg.turbulenceSecondaryScale = 2.85f;
    ambientCfg.turbulenceSecondaryMix = 0.44f;
    ambientCfg.homeSpringStrength = 0.085f;
    ambientCfg.linearDragPerSecond = 0.38f;
    ambientCfg.quadraticDragCoefficient = 0.0068f;
    ambientCfg.windAcceleration = ri::math::Vec3{0.28f, 0.0f, -0.12f};
    ambientCfg.scaleMin = 0.035f;
    ambientCfg.scaleMax = 0.11f;
    ambientCfg.scaleLifetimeExponent = 0.38f;
    ambientCfg.spinAngularVelocityMin = ri::math::Vec3{-72.0f, -95.0f, -72.0f};
    ambientCfg.spinAngularVelocityMax = ri::math::Vec3{72.0f, 95.0f, 72.0f};
    ambientCfg.seed = 0xF10A7EEDu;
    run.particlesAmbient.emplace(ambientCfg);

    const int matAmbient = run.scene.AddMaterial(ri::scene::Material{
        .name = "ShowcaseAmbientParticleMat",
        .shadingModel = ri::scene::ShadingModel::Unlit,
        .baseColor = ri::math::Vec3{0.35f, 0.72f, 0.95f},
        .emissiveColor = ri::math::Vec3{0.12f, 0.35f, 0.55f},
        .opacity = 0.28f,
        .additiveBlend = true,
    });
    run.particleAmbientBatch = run.scene.AddMeshInstanceBatch(ri::scene::MeshInstanceBatch{
        .name = "ShowcaseAmbientParticles",
        .parent = root,
        .mesh = mesh,
        .material = matAmbient,
        .transforms = {},
    });

    run.lastTick = std::chrono::steady_clock::now();
}

void TickShowcase(ShowcaseRuntime& run,
                  const float dt,
                  const double musicTimeSec,
                  const float rmsTight,
                  const float rmsWide,
                  const float transient,
                  const double wallSeconds) {
    if (run.audio != nullptr) {
        run.audio->Tick(static_cast<double>(dt) * 1000.0);
    }

    const float wide = std::max(rmsWide, 1.0e-4f);
    const float punch = std::clamp(rmsTight / wide, 0.0f, 6.0f);
    run.smoothRms += (rmsTight - run.smoothRms) * std::clamp(dt * 10.0f, 0.0f, 1.0f);
    run.peakTracker = std::max(run.peakTracker * std::exp(-dt * 2.8f), transient);

    run.phaseHue += dt * (0.35f + punch * 1.8f + run.smoothRms * 4.0f);

    auto hueToRgb = [](float h) -> ri::math::Vec3 {
        h = std::fmod(h, 6.28f);
        return ri::math::Vec3{
            0.5f + 0.5f * std::sin(h),
            0.5f + 0.5f * std::sin(h + 2.09f),
            0.5f + 0.5f * std::sin(h + 4.18f),
        };
    };

    const ri::math::Vec3 mood = hueToRgb(run.phaseHue);
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

    SetLightIntensity(run.scene, run.pointLights[0], 7.0f + run.smoothRms * 55.0f + punch * 18.0f);
    SetLightIntensity(run.scene, run.pointLights[1], 6.5f + transient * 70.0f + run.peakTracker * 25.0f);
    SetLightIntensity(run.scene, run.pointLights[2], 5.5f + wide * 35.0f);

    SetLightIntensity(run.scene, run.spotWarm, 28.0f + punch * 120.0f + run.peakTracker * 40.0f);
    SetLightIntensity(run.scene, run.spotCool, 24.0f + run.smoothRms * 90.0f);

    if (run.spotWarm != ri::scene::kInvalidHandle) {
        auto& n = run.scene.GetNode(run.spotWarm);
        n.localTransform.rotationDegrees.y =
            -28.0f + static_cast<float>(std::sin(wallSeconds * 1.7 + punch)) * 10.0f;
        n.localTransform.rotationDegrees.x =
            -72.0f + static_cast<float>(std::sin(wallSeconds * 2.1)) * 6.0f;
    }
    if (run.spotCool != ri::scene::kInvalidHandle) {
        auto& n = run.scene.GetNode(run.spotCool);
        n.localTransform.rotationDegrees.y =
            26.0f + static_cast<float>(std::cos(wallSeconds * 1.5 + run.smoothRms * 6.0f)) * 12.0f;
    }

    ri::scene::OrbitCameraState oc = run.orbit.orbit;
    const float t = static_cast<float>(wallSeconds);
    const float m = static_cast<float>(musicTimeSec);
    oc.yawDegrees = 132.0f + std::sin(t * 0.31f) * 44.0f + std::sin(m * 0.078f) * 26.0f
        + std::sin(t * 1.65f + punch * 2.1f) * 10.0f;
    oc.pitchDegrees = std::clamp(
        -14.0f + run.smoothRms * 11.0f + std::sin(t * 0.42f) * 7.0f + std::cos(m * 0.048f) * 5.5f,
        -32.0f,
        -7.5f);
    oc.distance = std::clamp(
        14.0f + std::sin(t * 0.21f) * 2.5f + punch * 1.35f + run.smoothRms * 1.6f
            + std::sin(m * 0.031f) * 1.1f,
        12.5f,
        18.0f);
    ri::scene::SetOrbitCameraState(run.scene, run.orbit, oc);

    if (run.particles.has_value()) {
        run.particles->Step(dt);
        run.particles->ApplyInstanceTransforms(run.scene, run.particleBatch);
    }
    if (run.particlesAmbient.has_value()) {
        run.particlesAmbient->Step(dt);
        run.particlesAmbient->ApplyInstanceTransforms(run.scene, run.particleAmbientBatch);
    }
    ri::scene::MeshInstanceBatch& batch = run.scene.GetMeshInstanceBatch(run.particleBatch);
    const float pulse = 1.0f + run.smoothRms * 2.4f + punch * 0.65f + run.peakTracker * 0.9f;
    for (ri::scene::Transform& tr : batch.transforms) {
        tr.scale = tr.scale * pulse;
    }
    if (run.particleAmbientBatch != ri::scene::kInvalidHandle) {
        ri::scene::MeshInstanceBatch& ambBatch = run.scene.GetMeshInstanceBatch(run.particleAmbientBatch);
        const float pulseAmb = 1.0f + run.smoothRms * 1.65f + punch * 0.48f + run.peakTracker * 0.62f;
        for (ri::scene::Transform& tr : ambBatch.transforms) {
            tr.scale = tr.scale * pulseAmb;
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    const ri::core::CommandLine commandLine(argc, argv);
    if (commandLine.HasFlag("--help") || commandLine.HasFlag("-h")) {
        ri::core::LogInfo("RawIron.ParticleShowcase — dev-room particle field driven by music RMS.");
        ri::core::LogInfo("  --workspace=<path>   Repository root (auto-detected when omitted)");
        ri::core::LogInfo("  --audio=<path>       WAV file (PCM 16-bit). Default: <workspace>/Assets/audio/Porter Robinson - Polygon Dust.wav");
        ri::core::LogInfo("  --width / --height   Window size (default 1600x900)");
        ri::core::LogInfo("  --silent             Visualizer motion without decoding WAV (time-based only)");
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

    const int width = std::clamp(commandLine.GetIntOr("--width", 1600), 480, 3840);
    const int height = std::clamp(commandLine.GetIntOr("--height", 900), 270, 2160);
    const bool silent = commandLine.HasFlag("--silent");

    auto runtime = std::make_shared<ShowcaseRuntime>();
    BuildDevRoom(*runtime);

    if (!silent && fs::exists(audioPath)) {
        if (!DecodeWavFilePcm16Mono(audioPath, runtime->pcm)) {
            ri::core::LogInfo("Could not decode WAV for analysis; continuing with time-based motion only.");
            runtime->pcm.valid = false;
        }
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
            runtime->music = runtime->audio->CreateManagedSound(audioPath.string(), 0.82, false, 1.0);
            if (runtime->music != nullptr) {
                runtime->music->Play();
            }
        }
    } else if (!audioErr.empty()) {
        ri::core::LogInfo("Audio backend: " + audioErr);
    }

    const fs::path textureRoot = workspace / "Assets" / "Textures";
    const fs::path frameTextureRoot = fs::exists(textureRoot) ? textureRoot : fs::path{};
    const ri::render::vulkan::VulkanPreviewWindowOptions windowOptions{
        .windowTitle = "RawIron Particle Showcase — Polygon Dust dev room",
        .textureRoot = frameTextureRoot,
        .enableHybridHdrPresentation = true,
    };

    const auto wallStart = std::chrono::steady_clock::now();

    const ri::render::vulkan::VulkanNativeSceneFrameCallback buildFrame =
        [runtime, wallStart, frameTextureRoot](ri::render::vulkan::VulkanNativeSceneFrame& frame, std::string*) {
#if defined(_WIN32)
            if ((GetAsyncKeyState(VK_ESCAPE) & 0x0001) != 0) {
                PostQuitMessage(0);
            }
#endif
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::clamp(
                std::chrono::duration<float>(now - runtime->lastTick).count(),
                1.0f / 240.0f,
                1.0f / 30.0f);
            runtime->lastTick = now;

            double musicTime = 0.0;
            if (runtime->music != nullptr && runtime->music->IsPlaying()) {
                musicTime = runtime->music->GetCurrentTime();
            }

            const double wallSeconds = std::chrono::duration<double>(now - wallStart).count();

            float rmsTight = 0.12f;
            float rmsWide = 0.08f;
            float transient = 0.0f;
            if (runtime->pcm.valid) {
                rmsTight = WindowRms(runtime->pcm, musicTime, 900);
                rmsWide = WindowRms(runtime->pcm, musicTime, 6000);
                transient = std::max(0.0f, rmsTight - rmsWide * 1.25f);
            } else {
                const float t = static_cast<float>(wallSeconds);
                rmsTight = 0.08f + 0.06f * std::sin(t * 6.2f) + 0.04f * std::sin(t * 2.1f);
                rmsWide = 0.06f + 0.03f * std::sin(t * 0.8f);
                transient = std::max(0.0f, rmsTight - rmsWide);
            }

            TickShowcase(*runtime, dt, musicTime, rmsTight, rmsWide, transient, wallSeconds);

            frame.scene = &runtime->scene;
            frame.cameraNode = runtime->orbit.cameraNode;
            frame.textureRoot = frameTextureRoot;
            frame.animationTimeSeconds = musicTime > 0.0 ? musicTime : wallSeconds;
            frame.renderQualityTier = 2;
            frame.renderExposure = 1.08f + runtime->smoothRms * 1.25f + runtime->peakTracker * 0.42f;
            frame.renderContrast = 1.03f + runtime->smoothRms * 0.07f;
            frame.renderSaturation = 1.06f + runtime->peakTracker * 0.14f;
            frame.renderFogDensity = 0.0018f + runtime->smoothRms * 0.007f;
            frame.postProcess.timeSeconds = static_cast<float>(wallSeconds);
            frame.postProcess.barrelDistortion =
                0.012f + runtime->smoothRms * 0.038f + transient * 0.045f + runtime->peakTracker * 0.02f;
            frame.postProcess.noiseAmount = 0.012f + runtime->peakTracker * 0.048f + runtime->smoothRms * 0.015f;
            frame.postProcess.scanlineAmount = 0.008f + runtime->smoothRms * 0.038f + transient * 0.025f;
            frame.postProcess.chromaticAberration = 0.0018f + transient * 0.045f;
            frame.postProcess.blurAmount = runtime->smoothRms * 0.008f + transient * 0.012f;
            frame.postProcess.tintStrength = 0.06f + runtime->smoothRms * 0.2f;
            const float hue = runtime->phaseHue;
            frame.postProcess.tintColor = ri::math::Vec3{
                0.88f + 0.14f * std::sin(hue),
                0.90f + 0.12f * std::sin(hue + 2.1f),
                1.02f + 0.14f * std::sin(hue + 4.2f),
            };
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
