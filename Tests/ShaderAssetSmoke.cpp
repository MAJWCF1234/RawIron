#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/ShaderAsset.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Expected workspace root argument.\n";
        return 1;
    }
    const fs::path workspaceRoot(argv[1]);

    struct ExpectedManifest {
        const char* game;
        std::size_t entries;
        bool expectsPost;
    };
    const ExpectedManifest expected[] = {
        {"CubeTest", 2U, true},
        {"EditorUiSmoke", 1U, false},
        {"LiminalHall", 6U, true},
        {"RawIronMultiplayerSandbox", 2U, true},
        {"WildernessRuins", 4U, false},
    };

    for (const ExpectedManifest& item : expected) {
        const fs::path gameRoot = workspaceRoot / "Games" / item.game;
        std::vector<ri::content::RawIronShaderManifestEntry> entries;
        std::vector<std::string> errors;
        if (!ri::content::LoadRawIronShaderManifest(gameRoot, &entries, &errors)) {
            std::cerr << item.game << ": shader manifest failed: "
                      << (errors.empty() ? "unknown error" : errors.front()) << '\n';
            return 2;
        }
        if (entries.size() != item.entries) {
            std::cerr << item.game << ": unexpected shader entry count.\n";
            return 3;
        }
        for (const auto& entry : entries) {
            if (entry.key != entry.asset.id || entry.asset.assetPath.extension() != ".rishader") {
                std::cerr << item.game << ": manifest identity was not preserved.\n";
                return 4;
            }
            for (const auto& texture : entry.asset.textures) {
                if (texture.required && !fs::is_regular_file(texture.resolvedPath)) {
                    std::cerr << item.game << ": required texture was not resolved.\n";
                    return 5;
                }
            }
        }

        const auto presentation = ri::content::ComposeRawIronShaderPresentation(entries);
        if (presentation.has_value() != item.expectsPost) {
            std::cerr << item.game << ": post-process composition state was incorrect.\n";
            return 6;
        }
        if (presentation.has_value()
            && presentation->parameters.toneCurveStrength <= 0.0f
            && presentation->parameters.vignetteStrength <= 0.0f
            && presentation->parameters.casSharpenAmount <= 0.0f
            && presentation->parameters.outputDitherStrength <= 0.0f) {
            std::cerr << item.game << ": authored post-process values were not parsed.\n";
            return 7;
        }
        if (std::string_view(item.game) == "CubeTest"
            && (presentation->parameters.barbatosFakeHdrPreset != 0
                || presentation->parameters.barbatosFakeHdrStrength != 0.15f
                || presentation->parameters.riAdaptiveDebandStrength != 0.12f
                || presentation->parameters.riLocalSharpenStrength != 0.12f
                || presentation->parameters.riOutlineStrength != 0.10f)) {
            std::cerr << "CubeTest: native Raw Iron post effects were not composed.\n";
            return 14;
        }

        const auto manifest = ri::content::LoadGameManifest(gameRoot / "manifest.json");
        if (!manifest.has_value()) {
            std::cerr << item.game << ": game manifest failed to load.\n";
            return 8;
        }
        const std::vector<std::string> projectIssues = ri::content::ValidateGameProjectFormat(*manifest);
        if (!projectIssues.empty()) {
            std::cerr << item.game << ": native shader validation did not integrate with project validation: "
                      << projectIssues.front() << '\n';
            return 9;
        }
    }

    ri::content::RawIronShaderAsset cropResize{};
    std::string cropError;
    const fs::path cropPath = workspaceRoot / "Source" / "RawIron.Render.Vulkan" / "shaders"
        / "NativeEffects" / "crop_resize.rishader";
    if (!ri::content::LoadRawIronShaderAsset(cropPath, &cropResize, &cropError)) {
        std::cerr << "Native CropResize asset failed: " << cropError << '\n';
        return 10;
    }
    if (cropResize.domain != ri::content::RawIronShaderDomain::PostProcess
        || !cropResize.presentation.loaded
        || cropResize.presentation.parameters.cropScaleFilter != 0
        || cropResize.presentation.parameters.cropScaleStrength != 0.0f) {
        std::cerr << "Native CropResize parameters were not parsed deterministically.\n";
        return 11;
    }

    ri::content::RawIronShaderAsset fakeHdr{};
    const fs::path fakeHdrPath = workspaceRoot / "Source" / "RawIron.Render.Vulkan" / "shaders"
        / "NativeEffects" / "barbatos_fake_hdr.rishader";
    if (!ri::content::LoadRawIronShaderAsset(fakeHdrPath, &fakeHdr, &cropError)) {
        std::cerr << "Native Barbatos Fake HDR asset failed: " << cropError << '\n';
        return 12;
    }
    if (!fakeHdr.presentation.loaded
        || fakeHdr.presentation.parameters.barbatosFakeHdrPreset != 2
        || fakeHdr.presentation.parameters.barbatosFakeHdrStrength != 1.0f
        || fakeHdr.textures.size() != 1U
        || fakeHdr.textures.front().srgb
        || !fs::is_regular_file(fakeHdr.textures.front().resolvedPath)) {
        std::cerr << "Native Barbatos Fake HDR parameters or texture were not resolved deterministically.\n";
        return 13;
    }

    const fs::path nativeEffectRoot = workspaceRoot / "Source" / "RawIron.Render.Vulkan" / "shaders" / "NativeEffects";
    ri::content::RawIronShaderAsset adaptiveDeband{};
    ri::content::RawIronShaderAsset localSharpen{};
    ri::content::RawIronShaderAsset inkOutline{};
    if (!ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_adaptive_deband.rishader", &adaptiveDeband, &cropError)
        || !ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_local_sharpen.rishader", &localSharpen, &cropError)
        || !ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_ink_outline.rishader", &inkOutline, &cropError)) {
        std::cerr << "Raw Iron native post capability failed to load: " << cropError << '\n';
        return 15;
    }
    if (adaptiveDeband.presentation.parameters.riAdaptiveDebandIterations != 2
        || adaptiveDeband.presentation.parameters.riAdaptiveDebandStrength != 0.65f
        || localSharpen.presentation.parameters.riLocalSharpenStrength != 0.55f
        || localSharpen.presentation.parameters.riLocalSharpenEdgeLimit != 0.65f
        || inkOutline.presentation.parameters.riOutlineMethod != 3
        || inkOutline.presentation.parameters.riOutlineStrength != 0.75f
        || inkOutline.presentation.parameters.riOutlineThickness != 1.5f) {
        std::cerr << "Raw Iron native post capability parameters were not parsed deterministically.\n";
        return 16;
    }

    std::cout << "Validated native .rishader assets and manifest composition.\n";
    return 0;
}
