#pragma once

#include "RawIron/Scene/Scene.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace ri::scene {

/// Stable logical string ids → scene node handles with explicit lifetime and streaming-friendly weak refs.
///
/// Use \ref Register when content loads or spawns an addressable object; \ref Unregister when unloading.
/// \ref WeakRef captures a logical generation; if the slot is replaced or removed, \ref TryResolveWeak fails.
class LevelObjectRegistry {
public:
    struct WeakRef {
        std::string id{};
        /// Monotonic per-slot stamp; `0` means “never existed” / unknown capture.
        std::uint64_t generation = 0U;
    };

    /// Associates \p id with \p nodeHandle (must be a valid node index for the active \ref Scene when used).
    /// Duplicate ids replace the previous binding and bump the generation (invalidates older \ref WeakRef values).
    void Register(std::string id, int nodeHandle);

    /// Removes \p id if present (weak resolves fail afterward).
    void Unregister(std::string_view id);

    void Clear() noexcept;

    [[nodiscard]] std::optional<int> TryResolveNode(std::string_view id) const;

    /// Capture current generation for weak/streaming checks (empty optional if id not registered).
    [[nodiscard]] std::optional<WeakRef> CaptureWeak(std::string_view id) const;

    /// Resolves only if the slot still matches the captured generation and points at a valid handle.
    [[nodiscard]] std::optional<int> TryResolveWeak(const WeakRef& weakRef) const;

    [[nodiscard]] std::size_t Size() const noexcept;

    /// Rebuilds the map from scene nodes with non-empty \ref Node::name (later nodes win on duplicate names).
    void RebuildFromNamedSceneNodes(const Scene& scene);

    /// Drops entries whose handles are no longer valid for \p scene (count or detached indices).
    [[nodiscard]] std::size_t PruneInvalidHandles(const Scene& scene);

    LevelObjectRegistry();
    ~LevelObjectRegistry();

    LevelObjectRegistry(const LevelObjectRegistry&) = delete;
    LevelObjectRegistry& operator=(const LevelObjectRegistry&) = delete;
    LevelObjectRegistry(LevelObjectRegistry&&) noexcept;
    LevelObjectRegistry& operator=(LevelObjectRegistry&&) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ri::scene
