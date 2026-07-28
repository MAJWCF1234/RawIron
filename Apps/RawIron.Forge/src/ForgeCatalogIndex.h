#pragma once

#include "ForgeCatalog.h"

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace ri::forge {

struct CatalogIndexResult {
    std::uint64_t generation = 0;
    AssetCatalog catalog{};
    std::filesystem::path preferredSelection{};
    double elapsedMilliseconds = 0.0;
    std::string error{};
};

/// Persistent coalescing worker used by Forge so recursive asset discovery never blocks the UI thread.
class AsyncAssetCatalogIndex {
public:
    explicit AsyncAssetCatalogIndex(std::filesystem::path workspaceRoot);
    ~AsyncAssetCatalogIndex();

    AsyncAssetCatalogIndex(const AsyncAssetCatalogIndex&) = delete;
    AsyncAssetCatalogIndex& operator=(const AsyncAssetCatalogIndex&) = delete;

    std::uint64_t Request(std::filesystem::path preferredSelection = {});
    [[nodiscard]] std::optional<CatalogIndexResult> Poll();
    [[nodiscard]] bool Busy() const;

private:
    void Run(std::stop_token stopToken);

    std::filesystem::path workspaceRoot_{};
    mutable std::mutex mutex_{};
    std::condition_variable_any wake_{};
    std::jthread worker_{};
    bool requestPending_ = false;
    bool busy_ = false;
    std::uint64_t requestedGeneration_ = 0;
    std::filesystem::path preferredSelection_{};
    std::optional<CatalogIndexResult> completed_{};
};

} // namespace ri::forge
