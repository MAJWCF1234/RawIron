#pragma once

#include "RawIron/DataSchema/SchemaRegistry.h"

#include <optional>
#include <vector>

namespace ri::data::schema {

struct MigrationEdge {
    SchemaId from{};
    SchemaId to{};
};

/// Directed migration steps between document schema versions (decode → migrate chain → validate).
class MigrationRegistry {
public:
    void AddEdge(MigrationEdge edge);
    [[nodiscard]] bool CanReach(SchemaId from, SchemaId to) const;
    /// Ordered chain from `from` to `to` inclusive, or empty if unreachable.
    [[nodiscard]] std::optional<std::vector<SchemaId>> ShortestPath(SchemaId from, SchemaId to) const;
    void Clear();

private:
    std::vector<MigrationEdge> edges_{};
};

} // namespace ri::data::schema
