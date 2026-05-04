#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/CrashDiagnostics.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/SoftwarePreview.h"
#include "RawIron/Render/VulkanPreviewPresenter.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneKit.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
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
#endif

namespace {

namespace fs = std::filesystem;

void PrintPreviewUsage() {
    std::cout
        << "RawIron.Preview — Scene Kit software snapshot + optional Vulkan window (Windows).\n"
        << "\n"
        << "  --example <slug>        Scene Kit example slug (default: scene_controls_orbit)\n"
        << "  --width / --height      Output / window size (default 768)\n"
        << "  --headless | --save     Write BMP (see --output)\n"
        << "  --output <path>         BMP path (default: rawiron_preview.bmp)\n"
        << "  --backend auto|vulkan|vulkan-native   Preview loop backend\n"
        << "  --psx-water            Live RawIron-native PSX water package proof window\n"
        << "\nPhoto mode (FOV only; does not edit scene cameras):\n"
        << "  --photo-mode            Mild default widen (~1.18x vertical FOV) unless --photo-fov / --photo-scale\n"
        << "  --photo-fov <deg>       Absolute vertical FOV, or horizontal if --photo-horizontal\n"
        << "  --photo-scale <factor>  Multiply authored vertical FOV\n"
        << "  --photo-horizontal      Treat --photo-fov as horizontal FOV (requires --photo-fov)\n";
}

struct PsxWaterPreview {
    ri::scene::Scene scene{"PSXWaterPackageProof"};
    int cameraNode = ri::scene::kInvalidHandle;
    fs::path packageRoot{};
};

ri::scene::Mesh BuildReconstructedPsxWaterMesh(const fs::path& workspaceRoot);

std::vector<float> ExtractJsonFloatArrayFromLine(const std::string& line, const std::string& key) {
    const std::size_t keyPos = line.find(key);
    if (keyPos == std::string::npos) {
        return {};
    }
    const std::size_t open = line.find('[', keyPos);
    const std::size_t close = line.find(']', open);
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        return {};
    }

    std::vector<float> values;
    std::stringstream stream(line.substr(open + 1U, close - open - 1U));
    std::string item;
    while (std::getline(stream, item, ',')) {
        values.push_back(std::stof(item));
    }
    return values;
}

ri::scene::Mesh LoadRawIronWaterModelJson(const fs::path& modelPath) {
    std::ifstream in(modelPath, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open packaged RawIron water model: " + modelPath.string());
    }

    ri::scene::Mesh mesh{
        .name = "psx_water_packaged_mesh",
        .primitive = ri::scene::PrimitiveType::Custom,
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.find("\"position\"") != std::string::npos) {
            const std::vector<float> position = ExtractJsonFloatArrayFromLine(line, "\"position\"");
            const std::vector<float> uv = ExtractJsonFloatArrayFromLine(line, "\"uv\"");
            if (position.size() >= 3U) {
                mesh.positions.push_back(ri::math::Vec3{position[0], position[1], position[2]});
                mesh.texCoords.push_back(uv.size() >= 2U ? ri::math::Vec2{uv[0], uv[1]} : ri::math::Vec2{0.0f, 0.0f});
            }
            continue;
        }
        if (line.find("\"indices\"") != std::string::npos) {
            const std::vector<float> indices = ExtractJsonFloatArrayFromLine(line, "\"indices\"");
            mesh.indices.reserve(indices.size());
            for (const float index : indices) {
                mesh.indices.push_back(static_cast<int>(index));
            }
        }
    }

    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.indices.size());
    if (mesh.positions.empty() || mesh.indices.empty()) {
        throw std::runtime_error("Packaged RawIron water model did not contain renderable mesh data: " + modelPath.string());
    }
    return mesh;
}

struct AxisPair {
    int u = 0;
    int v = 1;
};

float AxisValue(const ri::math::Vec3& value, int axis) {
    if (axis == 0) {
        return value.x;
    }
    if (axis == 1) {
        return value.y;
    }
    return value.z;
}

AxisPair ChooseBroadestAxes(const ri::math::Vec3& extent) {
    struct AxisExtent {
        int axis = 0;
        float extent = 0.0f;
    };
    std::array<AxisExtent, 3> axes{{
        AxisExtent{0, extent.x},
        AxisExtent{1, extent.y},
        AxisExtent{2, extent.z},
    }};
    std::sort(axes.begin(), axes.end(), [](const AxisExtent& lhs, const AxisExtent& rhs) {
        return lhs.extent > rhs.extent;
    });
    return AxisPair{axes[0].axis, axes[1].axis};
}

ri::scene::Mesh ReconstructProvidedWaterMeshForPreview(const ri::scene::Scene& importedScene) {
    std::vector<ri::math::Vec3> sourcePositions;
    for (std::size_t nodeIndex = 0; nodeIndex < importedScene.NodeCount(); ++nodeIndex) {
        const ri::scene::Node& node = importedScene.GetNode(static_cast<int>(nodeIndex));
        if (node.mesh == ri::scene::kInvalidHandle) {
            continue;
        }
        const ri::scene::Mesh& mesh = importedScene.GetMesh(node.mesh);
        const ri::math::Mat4 world = importedScene.ComputeWorldMatrix(static_cast<int>(nodeIndex));
        for (const ri::math::Vec3& position : mesh.positions) {
            sourcePositions.push_back(ri::math::TransformPoint(world, position));
        }
    }
    if (sourcePositions.size() < 3U) {
        throw std::runtime_error("Provided PSX water mesh did not import enough vertices to reconstruct a surface.");
    }

    ri::math::Vec3 minPoint{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
    };
    ri::math::Vec3 maxPoint{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
    };
    for (const ri::math::Vec3& point : sourcePositions) {
        minPoint.x = std::min(minPoint.x, point.x);
        minPoint.y = std::min(minPoint.y, point.y);
        minPoint.z = std::min(minPoint.z, point.z);
        maxPoint.x = std::max(maxPoint.x, point.x);
        maxPoint.y = std::max(maxPoint.y, point.y);
        maxPoint.z = std::max(maxPoint.z, point.z);
    }

    const ri::math::Vec3 extent = maxPoint - minPoint;
    const AxisPair axes = ChooseBroadestAxes(extent);
    const float uExtent = std::max(0.0001f, AxisValue(extent, axes.u));
    const float vExtent = std::max(0.0001f, AxisValue(extent, axes.v));
    const float uCenter = (AxisValue(minPoint, axes.u) + AxisValue(maxPoint, axes.u)) * 0.5f;

    std::vector<ri::math::Vec3> positions;
    std::vector<ri::math::Vec2> texCoords;
    std::vector<int> indices;
    positions.reserve(sourcePositions.size());
    texCoords.reserve(sourcePositions.size());
    indices.reserve(sourcePositions.size());
    for (const ri::math::Vec3& point : sourcePositions) {
        const float u = (AxisValue(point, axes.u) - AxisValue(minPoint, axes.u)) / uExtent;
        const float v = (AxisValue(point, axes.v) - AxisValue(minPoint, axes.v)) / vExtent;
        const float centeredU = (AxisValue(point, axes.u) - uCenter) / uExtent;
        const float ripple = std::sin((u * 19.0f) + (v * 11.0f)) * 0.035f;
        positions.push_back(ri::math::Vec3{
            centeredU * 6.2f,
            (0.52f - v) * 2.2f + ripple,
            2.35f + (v * 3.25f),
        });
        texCoords.push_back(ri::math::Vec2{u, 1.0f - v});
        indices.push_back(static_cast<int>(indices.size()));
    }

    return ri::scene::Mesh{
        .name = "psx_water_provided_mesh_reconstructed_uv",
        .primitive = ri::scene::PrimitiveType::Custom,
        .vertexCount = static_cast<int>(positions.size()),
        .indexCount = static_cast<int>(indices.size()),
        .positions = std::move(positions),
        .texCoords = std::move(texCoords),
        .indices = std::move(indices),
    };
}

ri::scene::Mesh BuildReconstructedPsxWaterMesh(const fs::path& workspaceRoot) {
    ri::scene::Scene importedMeshScene("PSXWaterProvidedMeshImport");
    std::string importError;
    const int importedRoot = ri::scene::AddFbxModelNode(
        importedMeshScene,
        ri::scene::FbxModelOptions{
            .sourcePath = workspaceRoot / "Assets" / "Source" / "PSX_Water" / "SM_PSX_Water.FBX",
            .wrapperNodeName = "PSXWaterProvidedMesh",
        },
        &importError);
    if (importedRoot == ri::scene::kInvalidHandle) {
        throw std::runtime_error("Failed to import provided PSX water mesh: " + importError);
    }
    return ReconstructProvidedWaterMeshForPreview(importedMeshScene);
}

void WriteRawIronModelJson(const ri::scene::Mesh& mesh, const fs::path& outputPath) {
    fs::create_directories(outputPath.parent_path());
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to open RawIron model output: " + outputPath.string());
    }

    out << std::fixed << std::setprecision(6);
    out << "{\n";
    out << "  \"formatVersion\": 1,\n";
    out << "  \"modelId\": \"psx_water_surface\",\n";
    out << "  \"displayName\": \"PSX Water Surface\",\n";
    out << "  \"primitive\": \"custom\",\n";
    out << "  \"coordinateSystem\": \"rawiron\",\n";
    out << "  \"vertices\": [\n";
    for (std::size_t index = 0; index < mesh.positions.size(); ++index) {
        const ri::math::Vec3& p = mesh.positions[index];
        const ri::math::Vec2 uv = index < mesh.texCoords.size() ? mesh.texCoords[index] : ri::math::Vec2{0.0f, 0.0f};
        out << "    { \"position\": [" << p.x << ", " << p.y << ", " << p.z << "], \"uv\": ["
            << uv.x << ", " << uv.y << "] }";
        if (index + 1U < mesh.positions.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"indices\": [";
    for (std::size_t index = 0; index < mesh.indices.size(); ++index) {
        if (index > 0U) {
            out << ", ";
        }
        out << mesh.indices[index];
    }
    out << "],\n";
    out << "  \"material\": \"materials/psx_water.ri_material.json\",\n";
    out << "  \"reconstruction\": {\n";
    out << "    \"status\": \"rawiron-native\",\n";
    out << "    \"basis\": [\n";
    out << "      \"paid source geometry reconstructed into RawIron mesh data\",\n";
    out << "      \"package-owned UVs for the caustics atlas\",\n";
    out << "      \"runtime does not require third-party authoring containers\"\n";
    out << "    ]\n";
    out << "  }\n";
    out << "}\n";
}

PsxWaterPreview BuildPsxWaterPreview(const fs::path& workspaceRoot) {
    PsxWaterPreview preview{};
    preview.packageRoot = workspaceRoot / "Assets" / "Packages" / "PSX_Water";

    ri::scene::Scene& scene = preview.scene;
    const int root = scene.CreateNode("World");
    preview.cameraNode = scene.CreateNode("Camera", root);
    const int camera = scene.AddCamera(ri::scene::Camera{
        .name = "ProofCamera",
        .projection = ri::scene::ProjectionType::Perspective,
        .fieldOfViewDegrees = 55.0f,
        .nearClip = 0.1f,
        .farClip = 100.0f,
    });
    scene.AttachCamera(preview.cameraNode, camera);
    scene.GetNode(preview.cameraNode).localTransform.position = ri::math::Vec3{0.0f, 0.0f, -4.5f};

    const int material = scene.AddMaterial(ri::scene::Material{
        .name = "psx_water",
        .shadingModel = ri::scene::ShadingModel::Unlit,
        .baseColor = ri::math::Vec3{0.95f, 1.0f, 1.0f},
        .baseColorTexture = "T_PSX_Caustics_Atlas.png",
        .baseColorTextureAtlasColumns = 7,
        .baseColorTextureAtlasRows = 21,
        .baseColorTextureAtlasFrameCount = 147,
        .baseColorTextureAtlasFramesPerSecond = 12.0f,
        .emissiveColor = ri::math::Vec3{0.08f, 0.22f, 0.28f},
        .opacity = 1.0f,
        .doubleSided = true,
        .transparent = true,
        .emissiveTexture = "T_PSX_Caustics_Atlas.png",
        .opacityTexture = "T_PSX_Caustics_Atlas.png",
    });

    const int meshHandle = scene.AddMesh(LoadRawIronWaterModelJson(preview.packageRoot / "models" / "psx_water_surface.ri_model.json"));
    const int waterNode = scene.CreateNode("PSXWaterProvidedMeshReconstructedUvs", root);
    scene.GetNode(waterNode).localTransform.position = ri::math::Vec3{0.0f, 0.0f, 0.0f};
    scene.AttachMesh(waterNode, meshHandle, material);

    return preview;
}

void FillPreviewImageFromSoftware(const ri::render::software::SoftwareImage& source,
                                  ri::render::vulkan::PreviewImageData& frame) {
    frame.width = source.width;
    frame.height = source.height;
    frame.format = ri::render::vulkan::PreviewPixelFormat::Bgr8;
    frame.pixels.resize(static_cast<std::size_t>(source.width * source.height * 3));
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            const std::size_t src = static_cast<std::size_t>((y * source.width + x) * 3);
            const std::size_t dst = src;
            frame.pixels[dst + 0U] = source.pixels[src + 2U];
            frame.pixels[dst + 1U] = source.pixels[src + 1U];
            frame.pixels[dst + 2U] = source.pixels[src + 0U];
        }
    }
}

#if defined(_WIN32)
int PresentPsxWaterPreview(const fs::path& workspaceRoot, ri::render::software::ScenePreviewOptions options) {
    PsxWaterPreview preview = BuildPsxWaterPreview(workspaceRoot);
    options.textureRoot = preview.packageRoot;
    options.width = std::clamp(options.width, 64, 2048);
    options.height = std::clamp(options.height, 64, 2048);
    options.clearTop = ri::math::Vec3{0.0f, 0.0f, 0.0f};
    options.clearBottom = ri::math::Vec3{0.0f, 0.0f, 0.0f};
    options.fogColor = ri::math::Vec3{0.0f, 0.0f, 0.0f};
    options.pointSampleTextures = true;
    options.orderedDither = false;

    ri::render::software::ScenePreviewCache cache{};
    ri::render::software::SoftwareImage image{};
    const auto started = std::chrono::steady_clock::now();
    std::string error;
    const ri::render::vulkan::VulkanPreviewWindowOptions windowOptions{
        .windowTitle = "RawIron Package Proof - PSX Water (Native .ripak)",
    };
    const bool ok = ri::render::vulkan::RunVulkanSoftwarePreviewLoop(
        options.width,
        options.height,
        options.width,
        options.height,
        [&](ri::render::vulkan::PreviewImageData& frame) {
            const auto now = std::chrono::steady_clock::now();
            options.animationTimeSeconds = std::chrono::duration<double>(now - started).count();
            ri::render::software::RenderScenePreviewInto(preview.scene, preview.cameraNode, options, image, &cache);
            FillPreviewImageFromSoftware(image, frame);
        },
        windowOptions,
        &error);
    if (!ok) {
        throw std::runtime_error("PSX water preview failed: " + error);
    }
    return 0;
}
#endif

float ParsePositiveNumber(const char* label, const std::string& raw) {
    const float value = std::stof(raw);
    if (!(value > 0.0f)) {
        throw std::runtime_error(std::string(label) + " must be a positive number.");
    }
    return value;
}

enum class PreviewBackend {
    Auto,
    Vulkan,
    VulkanNative,
};

const char* PreviewBackendName(const PreviewBackend backend) {
    switch (backend) {
    case PreviewBackend::Auto:
        return "auto";
    case PreviewBackend::Vulkan:
        return "vulkan";
    case PreviewBackend::VulkanNative:
        return "vulkan-native";
    }
    return "unknown";
}

ri::render::software::ScenePreviewOptions BuildPreviewOptions(const ri::core::CommandLine& commandLine) {
    ri::render::software::ScenePreviewOptions options{};
    options.width = std::max(64, commandLine.GetIntOr("--width", 768));
    options.height = std::max(64, commandLine.GetIntOr("--height", 768));

    const std::optional<std::string> photoFov = commandLine.GetValue("--photo-fov");
    const std::optional<std::string> photoScale = commandLine.GetValue("--photo-scale");

    if (photoFov.has_value()) {
        options.photoMode.enabled = true;
        options.photoMode.fieldOfViewDegreesOverride = ParsePositiveNumber("--photo-fov", *photoFov);
    }
    if (photoScale.has_value()) {
        options.photoMode.enabled = true;
        options.photoMode.fieldOfViewScale = ParsePositiveNumber("--photo-scale", *photoScale);
    }
    if (commandLine.HasFlag("--photo-horizontal")) {
        if (!photoFov.has_value()) {
            throw std::runtime_error("--photo-horizontal requires --photo-fov.");
        }
        options.photoMode.fieldOfViewOverrideIsHorizontal = true;
    }
    if (commandLine.HasFlag("--photo-mode")) {
        options.photoMode.enabled = true;
        if (!photoFov.has_value() && !photoScale.has_value()) {
            options.photoMode.fieldOfViewScale = 1.18f;
        }
    }

    return options;
}

fs::path ResolveOutputPath(const ri::core::CommandLine& commandLine) {
    if (const auto output = commandLine.GetValue("--output"); output.has_value()) {
        return fs::path(*output);
    }
    return fs::current_path() / "rawiron_preview.bmp";
}

bool ShouldSavePreview(const ri::core::CommandLine& commandLine) {
    return commandLine.HasFlag("--headless") ||
           commandLine.HasFlag("--save") ||
           commandLine.GetValue("--output").has_value();
}

bool SavePreviewIfRequested(const ri::core::CommandLine& commandLine,
                            const ri::render::software::SoftwareImage& image,
                            fs::path& outputPath) {
    if (!ShouldSavePreview(commandLine)) {
        return false;
    }

    outputPath = ResolveOutputPath(commandLine);
    fs::create_directories(outputPath.parent_path());
    if (!ri::render::software::SaveBmp(image, outputPath.string())) {
        throw std::runtime_error("Failed to save preview image.");
    }
    return true;
}

bool LooksLikeWorkspaceRoot(const fs::path& path) {
    return fs::exists(path / "CMakeLists.txt") &&
           fs::exists(path / "Source") &&
           fs::exists(path / "Documentation");
}

fs::path DetectWorkspaceRoot(const ri::core::CommandLine& commandLine) {
    if (const auto rootArg = commandLine.GetValue("--root"); rootArg.has_value()) {
        return fs::weakly_canonical(fs::path(*rootArg));
    }

    fs::path current = fs::current_path();
    while (!current.empty()) {
        if (LooksLikeWorkspaceRoot(current)) {
            return current;
        }

        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    return fs::current_path();
}

PreviewBackend ParsePreviewBackend(const ri::core::CommandLine& commandLine) {
    const std::optional<std::string> backendArg = commandLine.GetValue("--backend");
    if (!backendArg.has_value()) {
        return PreviewBackend::Auto;
    }

    if (*backendArg == "vulkan") {
        return PreviewBackend::Vulkan;
    }
    if (*backendArg == "vulkan-native") {
        return PreviewBackend::VulkanNative;
    }
    if (*backendArg == "auto") {
        return PreviewBackend::Auto;
    }

    throw std::runtime_error("Unknown preview backend: " + *backendArg);
}

std::string ResolveExampleSlug(const ri::core::CommandLine& commandLine) {
    if (const auto exampleArg = commandLine.GetValue("--example"); exampleArg.has_value()) {
        return *exampleArg;
    }
    if (const auto sceneKitArg = commandLine.GetValue("--scenekit-example"); sceneKitArg.has_value()) {
        return *sceneKitArg;
    }
    return "scene_controls_orbit";
}

ri::scene::SceneKitPreview LoadSelectedPreview(const ri::core::CommandLine& commandLine, const fs::path& workspaceRoot) {
    const std::string slug = ResolveExampleSlug(commandLine);
    const ri::scene::SceneKitMilestoneOptions options{
        .assetRoot = workspaceRoot / "Assets" / "Source",
    };
    if (const std::optional<ri::scene::SceneKitPreview> preview = ri::scene::BuildSceneKitPreview(slug, options);
        preview.has_value()) {
        return *preview;
    }

    throw std::runtime_error("Unknown Scene Kit example slug: " + slug);
}

#if defined(_WIN32)
int PresentInteractivePreview(PreviewBackend backend,
                              const ri::scene::SceneKitPreview& preview,
                              const ri::render::software::ScenePreviewOptions& previewOptions) {
    const std::string windowTitle = "RawIron Preview - " + preview.title;
    std::string error;
    const ri::render::vulkan::VulkanPreviewWindowOptions windowOptions{
        .windowTitle = windowTitle,
        .scenePhotoMode = previewOptions.photoMode,
    };
    const int width = std::max(previewOptions.width, 1);
    const int height = std::max(previewOptions.height, 1);
    bool presented = false;
    if (backend == PreviewBackend::VulkanNative) {
        presented = ri::render::vulkan::PresentSceneKitPreviewWindowNative(preview, width, height, windowOptions, &error);
    } else if (backend == PreviewBackend::Auto) {
        presented = ri::render::vulkan::PresentSceneKitPreviewWindowNative(preview, width, height, windowOptions, &error);
        if (!presented) {
            ri::core::LogInfo("Native Vulkan preview unavailable, falling back to the software-upload presenter: " + error);
            error.clear();
            presented = ri::render::vulkan::PresentSceneKitPreviewWindow(preview, width, height, windowOptions, &error);
        }
    } else {
        presented = ri::render::vulkan::PresentSceneKitPreviewWindow(preview, width, height, windowOptions, &error);
    }
    if (!presented) {
        throw std::runtime_error("Vulkan preview failed: " + error);
    }
    return 0;
}

#endif

} // namespace

int main(int argc, char** argv) {
    ri::core::InitializeCrashDiagnostics();
    try {
        const ri::core::CommandLine commandLine(argc, argv);
        if (commandLine.HasFlag("--help") || commandLine.HasFlag("-h")) {
            PrintPreviewUsage();
            return 0;
        }
        const PreviewBackend backend = ParsePreviewBackend(commandLine);
        const fs::path workspaceRoot = DetectWorkspaceRoot(commandLine);
        const ri::render::software::ScenePreviewOptions options = BuildPreviewOptions(commandLine);
        if (const auto modelOutput = commandLine.GetValue("--psx-water-export-model"); modelOutput.has_value()) {
            const ri::scene::Mesh mesh = BuildReconstructedPsxWaterMesh(workspaceRoot);
            WriteRawIronModelJson(mesh, fs::path(*modelOutput));
            ri::core::LogInfo("Wrote RawIron PSX water model: " + fs::path(*modelOutput).string());
            return 0;
        }
        if (commandLine.HasFlag("--psx-water")) {
#if defined(_WIN32)
            ri::core::LogSection("PSX Water Package Preview");
            ri::core::LogInfo("Opening live RawIron-native water proof window.");
            ri::core::LogInfo("Package: " + (workspaceRoot / "Assets" / "Packages" / "PSX_Water.ripak").string());
            ri::core::LogInfo("Close the window to exit.");
            return PresentPsxWaterPreview(workspaceRoot, options);
#else
            throw std::runtime_error("--psx-water live window is currently implemented on Windows.");
#endif
        }
        const ri::scene::SceneKitPreview preview = LoadSelectedPreview(commandLine, workspaceRoot);

        ri::render::vulkan::SceneKitPreviewRenderBridgeStats bridgeStats{};
        std::string bridgeError;
        const ri::scene::PhotoModeCameraOverrides* const bridgePhotoMode =
            ri::scene::PhotoModeFieldOfViewActive(options.photoMode) ? &options.photoMode : nullptr;
        if (!ri::render::vulkan::BuildSceneKitPreviewVulkanBridge(
                preview,
                options.width,
                options.height,
                bridgePhotoMode,
                &bridgeStats,
                &bridgeError)) {
            throw std::runtime_error("Scene-to-Vulkan bridge validation failed: " + bridgeError);
        }

        const ri::render::software::SoftwareImage image = ri::render::software::RenderScenePreview(
            preview.scene,
            preview.orbitCamera.cameraNode,
            options);

        ri::core::LogSection("Preview Startup");
        ri::core::LogInfo("Example: " + preview.slug + " [" + preview.statusLabel + "]");
        ri::core::LogInfo("Track: " + preview.rawIronTrack);
        ri::core::LogInfo("Bridge: commands=" + std::to_string(bridgeStats.renderCommandCount)
                          + " batches=" + std::to_string(bridgeStats.submissionBatchCount)
                          + " draws=" + std::to_string(bridgeStats.drawCommandCount)
                          + " intents=" + std::to_string(bridgeStats.intentCount));

        fs::path savedPath;
        const bool saved = SavePreviewIfRequested(commandLine, image, savedPath);
        if (commandLine.HasFlag("--headless")) {
            if (!saved) {
                throw std::runtime_error("Headless preview requires an output path or --save.");
            }
            ri::core::LogInfo("Headless preview saved: " + savedPath.string());
            ri::core::LogInfo("Image size: " + std::to_string(image.width) + "x" + std::to_string(image.height));
            return 0;
        }

#if defined(_WIN32)
        if (saved) {
            ri::core::LogInfo("Snapshot saved: " + savedPath.string());
        }
        ri::core::LogInfo("Opening preview window.");
        ri::core::LogInfo("Backend: " + std::string(PreviewBackendName(backend)));
        ri::core::LogInfo("Software preview remains headless-only for saved regression snapshots.");
        ri::core::LogInfo("Close the window to exit.");
        return PresentInteractivePreview(backend, preview, options);
#else
        if (!saved) {
            savedPath = fs::current_path() / "rawiron_preview.bmp";
            fs::create_directories(savedPath.parent_path());
            if (!ri::render::software::SaveBmp(image, savedPath.string())) {
                throw std::runtime_error("Failed to save preview image.");
            }
        }
        ri::core::LogInfo("Preview window is not implemented on this platform yet.");
        ri::core::LogInfo("Saved preview: " + savedPath.string());
        ri::core::LogInfo("Image size: " + std::to_string(image.width) + "x" + std::to_string(image.height));
        return 0;
#endif
    } catch (const std::exception&) {
        ri::core::LogCurrentExceptionWithStackTrace("Preview Failure");
        return 1;
    }
}
