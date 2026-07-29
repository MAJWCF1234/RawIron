#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/ShaderAsset.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Expected workspace root argument.\n";
        return 1;
    }
    const fs::path workspaceRoot(argv[1]);

    std::error_code relativeEc{};
    const fs::path relativeGameRoot = fs::relative(workspaceRoot / "Games" / "CubeTest", fs::current_path(), relativeEc);
    const fs::path detectedFromRelative =
        ri::content::DetectWorkspaceRoot(relativeEc ? fs::path("Games/CubeTest") : relativeGameRoot);
    std::error_code equivalentEc{};
    if (!fs::equivalent(detectedFromRelative, workspaceRoot, equivalentEc) || equivalentEc) {
        std::cerr << "Relative game roots do not resolve to the engine workspace.\n";
        return 20;
    }

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
    std::set<std::string, std::less<>> validatedManifestGames;

    for (const ExpectedManifest& item : expected) {
        const fs::path gameRoot = workspaceRoot / "Games" / item.game;
        std::vector<ri::content::RawIronShaderManifestEntry> entries;
        std::vector<std::string> errors;
        if (!ri::content::LoadRawIronShaderManifest(gameRoot, &entries, &errors)) {
            std::cerr << item.game << ": shader manifest failed: "
                      << (errors.empty() ? "unknown error" : errors.front()) << '\n';
            return 2;
        }
        validatedManifestGames.insert(item.game);
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

    // Keep the fixed expectations above for known samples, but also discover every future game
    // manifest so adding a project cannot bypass native shader validation by omitting this test list.
    std::error_code discoveryError;
    for (const fs::directory_entry& game :
         fs::directory_iterator(workspaceRoot / "Games", fs::directory_options::skip_permission_denied, discoveryError)) {
        if (discoveryError || !game.is_directory()) {
            continue;
        }
        const fs::path manifestPath = game.path() / "assets" / "shaders.manifest";
        if (!fs::is_regular_file(manifestPath, discoveryError)
            || validatedManifestGames.contains(game.path().filename().string())) {
            discoveryError.clear();
            continue;
        }
        std::vector<ri::content::RawIronShaderManifestEntry> entries;
        std::vector<std::string> errors;
        if (!ri::content::LoadRawIronShaderManifest(game.path(), &entries, &errors) || entries.empty()) {
            std::cerr << game.path().filename().string() << ": discovered shader manifest failed: "
                      << (errors.empty() ? "unknown error" : errors.front()) << '\n';
            return 21;
        }
    }
    if (discoveryError) {
        std::cerr << "Unable to enumerate game shader manifests: " << discoveryError.message() << '\n';
        return 22;
    }

    std::size_t discoveredShaderAssets = 0U;
    for (fs::recursive_directory_iterator asset(
             workspaceRoot / "Games", fs::directory_options::skip_permission_denied, discoveryError), end;
         asset != end;
         asset.increment(discoveryError)) {
        if (discoveryError) {
            std::cerr << "Unable to enumerate game shader assets: " << discoveryError.message() << '\n';
            return 23;
        }
        std::string extension = asset->path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (!asset->is_regular_file() || extension != ".rishader") {
            continue;
        }
        ++discoveredShaderAssets;
        ri::content::RawIronShaderAsset parsedAsset{};
        std::string assetError;
        if (!ri::content::LoadRawIronShaderAsset(asset->path(), &parsedAsset, &assetError)) {
            std::cerr << "Discovered native shader asset failed: " << assetError << '\n';
            return 24;
        }
    }
    if (discoveredShaderAssets == 0U) {
        std::cerr << "No game-native shader assets were discovered.\n";
        return 25;
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
        || !fakeHdr.textures.front().relativePath.starts_with("native://")
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

    ri::content::RawIronShaderAsset pd80Cinetools{};
    if (!ri::content::LoadRawIronShaderAsset(
            nativeEffectRoot / "pd80_cinetools_lut.rishader", &pd80Cinetools, &cropError)) {
        std::cerr << "PD80 native Cinetools LUT asset failed to load: " << cropError << '\n';
        return 17;
    }
    if (!pd80Cinetools.presentation.loaded
        || pd80Cinetools.presentation.parameters.pd80CltLutSelector != 0.0f
        || pd80Cinetools.presentation.parameters.pd80CltMixChroma != 1.0f
        || pd80Cinetools.presentation.parameters.pd80CltMixLuma != 1.0f
        || pd80Cinetools.presentation.parameters.pd80CltGamma != 1.0f
        || pd80Cinetools.textures.size() != 3U) {
        std::cerr << "PD80 native Cinetools LUT parameters were not parsed deterministically.\n";
        return 18;
    }
    for (const auto& texture : pd80Cinetools.textures) {
        if (texture.srgb || !texture.relativePath.starts_with("native://")
            || !fs::is_regular_file(texture.resolvedPath)) {
            std::cerr << "PD80 native Cinetools LUT texture was not resolved as linear native data.\n";
            return 19;
        }
    }

    ri::content::RawIronShaderAsset signalGlitch{};
    ri::content::RawIronShaderAsset nightVision{};
    ri::content::RawIronShaderAsset highPassSharpen{};
    if (!ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_signal_glitch.rishader", &signalGlitch, &cropError)
        || !ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_night_vision.rishader", &nightVision, &cropError)
        || !ri::content::LoadRawIronShaderAsset(
            nativeEffectRoot / "ri_high_pass_sharpen.rishader", &highPassSharpen, &cropError)) {
        std::cerr << "Raw Iron signal/night/high-pass asset failed to load: " << cropError << '\n';
        return 21;
    }
    if (signalGlitch.presentation.parameters.riSignalGlitchStrength != 0.65f
        || signalGlitch.presentation.parameters.riSignalGlitchBlockSize != 16.0f
        || signalGlitch.presentation.parameters.riSignalGlitchColorShiftPixels != 3.0f
        || signalGlitch.presentation.parameters.riSignalGlitchSpeed != 1.0f
        || nightVision.presentation.parameters.riNightVisionStrength != 0.85f
        || nightVision.presentation.parameters.riNightVisionGain != 1.5f
        || nightVision.presentation.parameters.riNightVisionNoise != 0.08f
        || nightVision.presentation.parameters.riNightVisionVignette != 0.65f
        || highPassSharpen.presentation.parameters.riLocalSharpenStrength != 0.45f) {
        std::cerr << "Raw Iron signal/night/high-pass parameters were not parsed deterministically.\n";
        return 22;
    }

    const char* libraryAssets[] = {
        "ri_blending.rishader",
        "ri_text_overlay.rishader",
        "ri_shader_macros.rishader",
        "ri_shader_contract.rishader",
        "ri_shader_ui_contract.rishader",
    };
    for (const char* assetName : libraryAssets) {
        ri::content::RawIronShaderAsset library{};
        if (!ri::content::LoadRawIronShaderAsset(nativeEffectRoot / assetName, &library, &cropError)
            || library.stage != "library") {
            std::cerr << "Raw Iron shader library asset failed to load: " << assetName << ": " << cropError << '\n';
            return 23;
        }
        for (const auto& texture : library.textures) {
            if (!texture.relativePath.starts_with("native://") || !fs::is_regular_file(texture.resolvedPath)) {
                std::cerr << "Raw Iron shader library texture was not resolved: " << assetName << '\n';
                return 24;
            }
        }
    }

    ri::content::RawIronShaderAsset hq4x{};
    ri::content::RawIronShaderAsset hslShift{};
    ri::content::RawIronShaderAsset levelsPlus{};
    ri::content::RawIronShaderAsset lightDof{};
    ri::content::RawIronShaderAsset magicBloom{};
    ri::content::RawIronShaderAsset uiMask{};
    if (!ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_hq4x.rishader", &hq4x, &cropError)
        || !ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_hsl_shift.rishader", &hslShift, &cropError)
        || !ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_levels_plus.rishader", &levelsPlus, &cropError)
        || !ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_light_dof.rishader", &lightDof, &cropError)
        || !ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_magic_bloom.rishader", &magicBloom, &cropError)
        || !ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_ui_mask.rishader", &uiMask, &cropError)) {
        std::cerr << "Raw Iron requested shader tranche failed to load: " << cropError << '\n';
        return 25;
    }
    if (hq4x.presentation.parameters.riHq4xStrength != 1.0f
        || hq4x.presentation.parameters.riHq4xRadiusPixels != 1.5f
        || hslShift.presentation.parameters.riHslShiftStrength != 1.0f
        || hslShift.presentation.parameters.riHslOrange.y != 0.50f
        || levelsPlus.presentation.parameters.riLevelsPlusStrength != 1.0f
        || levelsPlus.presentation.parameters.riLevelsPlusAcesMode != 0
        || levelsPlus.presentation.parameters.riLevelsPlusGamma.z != 1.0f
        || lightDof.presentation.parameters.riLightDofStrength != 1.0f
        || lightDof.presentation.parameters.riLightDofAutoFocus != 1.0f
        || lightDof.presentation.parameters.riLightDofNearChromatic != 1.0f
        || magicBloom.presentation.parameters.riMagicBloomStrength != 1.0f
        || magicBloom.presentation.parameters.riMagicBloomThresholdPower != 2.0f
        || magicBloom.textures.size() != 1U
        || !magicBloom.textures.front().srgb
        || uiMask.presentation.parameters.riUiMaskStrength != 1.0f
        || uiMask.presentation.parameters.riUiMaskBlue != 1.0f
        || uiMask.textures.size() != 1U
        || uiMask.textures.front().srgb) {
        std::cerr << "Raw Iron requested shader parameters or textures were not parsed deterministically.\n";
        return 26;
    }

    ri::content::RawIronShaderAsset luminanceThreshold{};
    ri::content::RawIronShaderAsset colorQuantize{};
    ri::content::RawIronShaderAsset kaleidoscope{};
    if (!ri::content::LoadRawIronShaderAsset(
            nativeEffectRoot / "ri_luminance_threshold.rishader", &luminanceThreshold, &cropError)
        || !ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_color_quantize.rishader", &colorQuantize, &cropError)
        || !ri::content::LoadRawIronShaderAsset(nativeEffectRoot / "ri_kaleidoscope.rishader", &kaleidoscope, &cropError)) {
        std::cerr << "Raw Iron CShade capability assets failed to load: " << cropError << '\n';
        return 27;
    }
    if (luminanceThreshold.presentation.parameters.riLuminanceThresholdStrength != 1.0f
        || luminanceThreshold.presentation.parameters.riLuminanceThreshold != 0.8f
        || luminanceThreshold.presentation.parameters.riLuminanceThresholdSoftness != 0.5f
        || colorQuantize.presentation.parameters.riColorQuantizeStrength != 1.0f
        || colorQuantize.presentation.parameters.riColorQuantizePixelate != 0.0f
        || colorQuantize.presentation.parameters.riColorQuantizeResolution.x != 128.0f
        || colorQuantize.presentation.parameters.riColorQuantizeDitherMode != 0
        || colorQuantize.presentation.parameters.riColorQuantizeLevels.z != 8.0f
        || kaleidoscope.presentation.parameters.riKaleidoscopeStrength != 1.0f
        || kaleidoscope.presentation.parameters.riKaleidoscopeSegments != 6.0f
        || kaleidoscope.presentation.parameters.riKaleidoscopeSymmetry != 1.0f
        || kaleidoscope.presentation.parameters.riKaleidoscopeZoom != 1.0f) {
        std::cerr << "Raw Iron CShade capability parameters were not parsed deterministically.\n";
        return 28;
    }

    std::cout << "Validated native .rishader assets and manifest composition.\n";
    return 0;
}
