#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Validation/Schemas.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ri::world {

struct RuntimeCheckpointSnapshot {
    std::string slot = "autosave";
    ri::validation::RuntimeCheckpointState state{};
    std::optional<ri::math::Vec3> playerPosition;
    std::optional<ri::math::Vec3> playerRotation;
};

struct CheckpointStartupOptions {
    bool startFromCheckpoint = false;
    std::string slot = "autosave";
    std::optional<std::string> queryString;
};

struct CheckpointStartupDecision {
    bool startFromCheckpoint = false;
    std::string slot = "autosave";
    std::optional<RuntimeCheckpointSnapshot> snapshot;
};

class FileCheckpointStore {
public:
    explicit FileCheckpointStore(std::filesystem::path rootDirectory);

    [[nodiscard]] bool Save(const RuntimeCheckpointSnapshot& snapshot, std::string* error = nullptr) const;
    [[nodiscard]] std::optional<RuntimeCheckpointSnapshot> Load(std::string_view slot, std::string* error = nullptr) const;
    [[nodiscard]] bool Clear(std::string_view slot, std::string* error = nullptr) const;

private:
    [[nodiscard]] std::filesystem::path SlotPath(std::string_view slot) const;

    std::filesystem::path rootDirectory_;
};

[[nodiscard]] CheckpointStartupOptions ParseCheckpointStartupOptions(std::string_view queryString);
[[nodiscard]] CheckpointStartupDecision ResolveCheckpointStartupDecision(
    const CheckpointStartupOptions& options,
    const FileCheckpointStore& store,
    std::string* error = nullptr);

} // namespace ri::world
