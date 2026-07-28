#pragma once

#include "ForgeCatalog.h"

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/Scene.h"

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace ri::forge {

struct ForgePreviewBuildResult {
    std::uint64_t generation = 0;
    std::filesystem::path assetPath{};
    ri::scene::Scene scene{"Raw Iron Forge 3D Model"};
    ri::scene::OrbitCameraHandles camera{};
    std::size_t renderableNodeCount = 0;
    bool assetLoaded = false;
    std::string status{};
    double elapsedMilliseconds = 0.0;
};

[[nodiscard]] ForgePreviewBuildResult BuildForgePreviewScene(
    const std::filesystem::path& assetPath,
    AssetKind kind,
    std::uint64_t generation = 0,
    std::uintmax_t maximumSourceBytes = 256ULL * 1024ULL * 1024ULL);

/// Coalescing source importer: rapid asset selection only publishes the newest completed preview.
class AsyncForgePreviewBuilder {
public:
    AsyncForgePreviewBuilder();
    ~AsyncForgePreviewBuilder();

    AsyncForgePreviewBuilder(const AsyncForgePreviewBuilder&) = delete;
    AsyncForgePreviewBuilder& operator=(const AsyncForgePreviewBuilder&) = delete;

    std::uint64_t Request(std::filesystem::path assetPath, AssetKind kind);
    [[nodiscard]] std::optional<ForgePreviewBuildResult> Poll();
    [[nodiscard]] bool Busy() const;

private:
    void Run(std::stop_token stopToken);

    mutable std::mutex mutex_{};
    std::condition_variable_any wake_{};
    std::jthread worker_{};
    bool requestPending_ = false;
    bool busy_ = false;
    std::uint64_t requestedGeneration_ = 0;
    std::filesystem::path requestedAssetPath_{};
    AssetKind requestedKind_ = AssetKind::ModelSource;
    std::optional<ForgePreviewBuildResult> completed_{};
};

} // namespace ri::forge
