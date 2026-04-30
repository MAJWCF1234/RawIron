#include "RawIron/Scene/LevelObjectRegistry.h"

#include <memory>
#include <unordered_map>

namespace ri::scene {

struct LevelObjectRegistry::Impl {
    struct Slot {
        int nodeHandle = kInvalidHandle;
        std::uint64_t generation = 0U;
    };

    std::unordered_map<std::string, Slot> byId{};
};

LevelObjectRegistry::LevelObjectRegistry() : impl_(std::make_unique<Impl>()) {}

LevelObjectRegistry::~LevelObjectRegistry() = default;

LevelObjectRegistry::LevelObjectRegistry(LevelObjectRegistry&&) noexcept = default;
LevelObjectRegistry& LevelObjectRegistry::operator=(LevelObjectRegistry&&) noexcept = default;

void LevelObjectRegistry::Register(std::string id, const int nodeHandle) {
    if (id.empty() || impl_ == nullptr) {
        return;
    }
    auto& slot = impl_->byId[std::move(id)];
    slot.generation += 1U;
    slot.nodeHandle = nodeHandle;
}

void LevelObjectRegistry::Unregister(const std::string_view id) {
    if (id.empty() || impl_ == nullptr) {
        return;
    }
    impl_->byId.erase(std::string(id));
}

void LevelObjectRegistry::Clear() noexcept {
    if (impl_ != nullptr) {
        impl_->byId.clear();
    }
}

std::optional<int> LevelObjectRegistry::TryResolveNode(const std::string_view id) const {
    if (id.empty() || impl_ == nullptr) {
        return std::nullopt;
    }
    const auto it = impl_->byId.find(std::string(id));
    if (it == impl_->byId.end()) {
        return std::nullopt;
    }
    const int handle = it->second.nodeHandle;
    if (handle == kInvalidHandle) {
        return std::nullopt;
    }
    return handle;
}

std::optional<LevelObjectRegistry::WeakRef> LevelObjectRegistry::CaptureWeak(const std::string_view id) const {
    if (id.empty() || impl_ == nullptr) {
        return std::nullopt;
    }
    const auto it = impl_->byId.find(std::string(id));
    if (it == impl_->byId.end()) {
        return std::nullopt;
    }
    return WeakRef{
        .id = it->first,
        .generation = it->second.generation,
    };
}

std::optional<int> LevelObjectRegistry::TryResolveWeak(const WeakRef& weakRef) const {
    if (weakRef.id.empty() || weakRef.generation == 0U || impl_ == nullptr) {
        return std::nullopt;
    }
    const auto it = impl_->byId.find(weakRef.id);
    if (it == impl_->byId.end()) {
        return std::nullopt;
    }
    if (it->second.generation != weakRef.generation) {
        return std::nullopt;
    }
    const int handle = it->second.nodeHandle;
    if (handle == kInvalidHandle) {
        return std::nullopt;
    }
    return handle;
}

std::size_t LevelObjectRegistry::Size() const noexcept {
    return impl_ == nullptr ? 0U : impl_->byId.size();
}

void LevelObjectRegistry::RebuildFromNamedSceneNodes(const Scene& scene) {
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->byId.clear();
    for (int index = 0; index < static_cast<int>(scene.NodeCount()); ++index) {
        const std::string& name = scene.GetNode(index).name;
        if (name.empty()) {
            continue;
        }
        Register(name, index);
    }
}

std::size_t LevelObjectRegistry::PruneInvalidHandles(const Scene& scene) {
    if (impl_ == nullptr) {
        return 0U;
    }
    std::size_t removed = 0U;
    for (auto it = impl_->byId.begin(); it != impl_->byId.end();) {
        const int handle = it->second.nodeHandle;
        const bool valid = handle >= 0 && static_cast<std::size_t>(handle) < scene.NodeCount();
        if (!valid) {
            it = impl_->byId.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

} // namespace ri::scene
