#include "ForgeCatalogIndex.h"

#include <chrono>
#include <exception>
#include <utility>

namespace ri::forge {

AsyncAssetCatalogIndex::AsyncAssetCatalogIndex(std::filesystem::path workspaceRoot)
    : workspaceRoot_(std::move(workspaceRoot)),
      worker_([this](const std::stop_token stopToken) { Run(stopToken); }) {}

AsyncAssetCatalogIndex::~AsyncAssetCatalogIndex() {
    worker_.request_stop();
    wake_.notify_all();
}

std::uint64_t AsyncAssetCatalogIndex::Request(std::filesystem::path preferredSelection) {
    std::scoped_lock lock(mutex_);
    ++requestedGeneration_;
    preferredSelection_ = std::move(preferredSelection);
    requestPending_ = true;
    busy_ = true;
    wake_.notify_one();
    return requestedGeneration_;
}

std::optional<CatalogIndexResult> AsyncAssetCatalogIndex::Poll() {
    std::scoped_lock lock(mutex_);
    if (!completed_.has_value()) {
        return std::nullopt;
    }
    std::optional<CatalogIndexResult> result = std::move(completed_);
    completed_.reset();
    return result;
}

bool AsyncAssetCatalogIndex::Busy() const {
    std::scoped_lock lock(mutex_);
    return busy_;
}

void AsyncAssetCatalogIndex::Run(const std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        std::uint64_t generation = 0;
        std::filesystem::path preferredSelection{};
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, stopToken, [this]() { return requestPending_; });
            if (stopToken.stop_requested()) {
                break;
            }
            generation = requestedGeneration_;
            preferredSelection = preferredSelection_;
            requestPending_ = false;
        }

        const auto started = std::chrono::steady_clock::now();
        AssetCatalog catalog{};
        std::string error{};
        try {
            catalog = ScanAssetCatalog(workspaceRoot_);
        } catch (const std::exception& exception) {
            error = exception.what();
        } catch (...) {
            error = "Unknown asset index failure.";
        }
        const double elapsedMilliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();

        std::scoped_lock lock(mutex_);
        if (generation == requestedGeneration_) {
            completed_ = CatalogIndexResult{
                .generation = generation,
                .catalog = std::move(catalog),
                .preferredSelection = std::move(preferredSelection),
                .elapsedMilliseconds = elapsedMilliseconds,
                .error = std::move(error),
            };
            busy_ = false;
        }
    }
}

} // namespace ri::forge
