#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/CrashDiagnostics.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/VulkanPreviewPresenter.h"
#include "RawIron/Scene/SceneKit.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
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
        << "\nPhoto mode (FOV only; does not edit scene cameras):\n"
        << "  --photo-mode            Mild default widen (~1.18x vertical FOV) unless --photo-fov / --photo-scale\n"
        << "  --photo-fov <deg>       Absolute vertical FOV, or horizontal if --photo-horizontal\n"
        << "  --photo-scale <factor>  Multiply authored vertical FOV\n"
        << "  --photo-horizontal      Treat --photo-fov as horizontal FOV (requires --photo-fov)\n";
}

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
