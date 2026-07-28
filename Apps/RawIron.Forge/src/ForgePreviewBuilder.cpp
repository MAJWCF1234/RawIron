#include "ForgePreviewBuilder.h"

#include "RawIron/Content/PrimitiveModelDocument.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/PrimitiveModelBake.h"
#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <system_error>
#include <utility>

namespace ri::forge {
namespace {

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

void AddPreviewStage(
    ForgePreviewBuildResult& result,
    const int previewRoot,
    const std::vector<int>& frameNodes) {
    (void)ri::scene::AddGridHelper(
        result.scene,
        ri::scene::GridHelperOptions{
            .nodeName = "ForgeGrid",
            .parent = previewRoot,
            .size = 12.0F,
        });
    (void)ri::scene::AddAxesHelper(
        result.scene,
        ri::scene::AxesHelperOptions{
            .nodeName = "ForgeAxes",
            .parent = previewRoot,
            .axisLength = 1.5F,
        });
    (void)ri::scene::AddLightNode(
        result.scene,
        ri::scene::LightNodeOptions{
            .nodeName = "ForgeKeyLight",
            .parent = previewRoot,
            .transform = ri::scene::Transform{
                .rotationDegrees = {-35.0F, -35.0F, 0.0F},
            },
            .light = ri::scene::Light{
                .name = "ForgeKeyLight",
                .type = ri::scene::LightType::Directional,
                .color = {1.0F, 0.92F, 0.82F},
                .intensity = 2.0F,
            },
        });
    (void)ri::scene::AddLightNode(
        result.scene,
        ri::scene::LightNodeOptions{
            .nodeName = "ForgeFillLight",
            .parent = previewRoot,
            .transform = ri::scene::Transform{
                .rotationDegrees = {25.0F, 145.0F, 0.0F},
            },
            .light = ri::scene::Light{
                .name = "ForgeFillLight",
                .type = ri::scene::LightType::Directional,
                .color = {0.58F, 0.72F, 1.0F},
                .intensity = 0.65F,
            },
        });
    result.camera = ri::scene::AddOrbitCamera(
        result.scene,
        ri::scene::OrbitCameraOptions{
            .rigName = "ForgeOrbit",
            .parent = previewRoot,
            .camera = ri::scene::Camera{
                .name = "ForgeCamera",
                .fieldOfViewDegrees = 55.0F,
                .nearClip = 0.02F,
                .farClip = 1000.0F,
            },
            .orbit = ri::scene::OrbitCameraState{
                .distance = 6.0F,
                .yawDegrees = 145.0F,
                .pitchDegrees = -18.0F,
            },
        });
    if (!frameNodes.empty()) {
        (void)ri::scene::FrameNodesWithOrbitCamera(
            result.scene, result.camera, frameNodes, 1.45F);
    }
}

} // namespace

ForgePreviewBuildResult BuildForgePreviewScene(
    const std::filesystem::path& assetPath,
    const AssetKind kind,
    const std::uint64_t generation,
    const std::uintmax_t maximumSourceBytes) {
    const auto started = std::chrono::steady_clock::now();
    ForgePreviewBuildResult result{};
    result.generation = generation;
    result.assetPath = assetPath;
    const int previewRoot = result.scene.CreateNode("Forge3DPreview");
    std::vector<int> frameNodes{};

    if (assetPath.empty()) {
        result.status = "Select a model to preview";
    } else if (kind == AssetKind::PrimitiveModel) {
        const auto document = ri::content::LoadPrimitiveModelDocument(assetPath);
        if (!document.has_value()) {
            result.status = "Primitive model preview could not be loaded";
        } else {
            const ri::scene::PrimitiveModelInstantiationResult instantiated =
                ri::scene::InstantiatePrimitiveModel(
                    result.scene,
                    previewRoot,
                    *document,
                    assetPath.parent_path());
            if (instantiated.valid) {
                frameNodes = instantiated.partNodes;
                result.assetLoaded = !frameNodes.empty();
                result.status = "Native grouped primitive preview";
            } else {
                result.status = instantiated.errors.empty()
                    ? "Primitive model produced no preview geometry"
                    : instantiated.errors.front();
            }
        }
    } else if (kind == AssetKind::Rig) {
        result.status = "Rig document selected";
    } else {
        const std::string extension = LowerAscii(assetPath.extension().string());
        if (extension == ".blend") {
            result.status = "Blender source requires export to FBX, glTF, GLB, or OBJ";
        } else {
            std::error_code error{};
            const std::uintmax_t sourceBytes = std::filesystem::file_size(assetPath, error);
            if (!error && sourceBytes > maximumSourceBytes) {
                result.status = "Source exceeds the interactive preview size limit; validate or open it directly";
            } else {
                std::string importError{};
                const int imported = ri::scene::AddModelNode(
                    result.scene,
                    ri::scene::ImportedModelOptions{
                        .sourcePath = assetPath,
                        .nodeName = assetPath.stem().string(),
                        .parent = previewRoot,
                        .snapMeshBaseToGround = true,
                        .createPlaceholderOnFailure = false,
                    },
                    &importError);
                if (imported != ri::scene::kInvalidHandle) {
                    frameNodes = ri::scene::CollectRenderableNodes(result.scene);
                    result.assetLoaded = !frameNodes.empty();
                    result.status = result.assetLoaded
                        ? "Imported source preview"
                        : "Importer returned no renderable geometry";
                } else {
                    result.status = importError.empty() ? "Source preview import failed" : importError;
                }
            }
        }
    }

    result.renderableNodeCount = frameNodes.size();
    AddPreviewStage(result, previewRoot, frameNodes);
    result.elapsedMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    return result;
}

AsyncForgePreviewBuilder::AsyncForgePreviewBuilder()
    : worker_([this](const std::stop_token stopToken) { Run(stopToken); }) {}

AsyncForgePreviewBuilder::~AsyncForgePreviewBuilder() {
    worker_.request_stop();
    wake_.notify_all();
}

std::uint64_t AsyncForgePreviewBuilder::Request(
    std::filesystem::path assetPath,
    const AssetKind kind) {
    std::scoped_lock lock(mutex_);
    ++requestedGeneration_;
    requestedAssetPath_ = std::move(assetPath);
    requestedKind_ = kind;
    requestPending_ = true;
    busy_ = true;
    wake_.notify_one();
    return requestedGeneration_;
}

std::optional<ForgePreviewBuildResult> AsyncForgePreviewBuilder::Poll() {
    std::scoped_lock lock(mutex_);
    if (!completed_.has_value()) {
        return std::nullopt;
    }
    std::optional<ForgePreviewBuildResult> result = std::move(completed_);
    completed_.reset();
    return result;
}

bool AsyncForgePreviewBuilder::Busy() const {
    std::scoped_lock lock(mutex_);
    return busy_;
}

void AsyncForgePreviewBuilder::Run(const std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        std::uint64_t generation = 0;
        std::filesystem::path assetPath{};
        AssetKind kind = AssetKind::ModelSource;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, stopToken, [this]() { return requestPending_; });
            if (stopToken.stop_requested()) {
                break;
            }
            generation = requestedGeneration_;
            assetPath = requestedAssetPath_;
            kind = requestedKind_;
            requestPending_ = false;
        }

        ForgePreviewBuildResult result{};
        try {
            result = BuildForgePreviewScene(assetPath, kind, generation);
        } catch (const std::exception& exception) {
            result = BuildForgePreviewScene({}, AssetKind::ModelSource, generation);
            result.assetPath = assetPath;
            result.status = std::string("Preview failed: ") + exception.what();
        } catch (...) {
            result = BuildForgePreviewScene({}, AssetKind::ModelSource, generation);
            result.assetPath = assetPath;
            result.status = "Preview failed with an unknown importer error";
        }
        std::scoped_lock lock(mutex_);
        if (generation == requestedGeneration_) {
            completed_ = std::move(result);
            busy_ = false;
        }
    }
}

} // namespace ri::forge
