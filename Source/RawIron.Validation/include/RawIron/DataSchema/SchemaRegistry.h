#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace ri::data::schema {

struct SchemaId {
    std::string kind;
    std::uint32_t versionMajor = 0;
    std::uint32_t versionMinor = 0;

    [[nodiscard]] friend bool operator==(const SchemaId& a, const SchemaId& b) {
        return a.kind == b.kind && a.versionMajor == b.versionMajor && a.versionMinor == b.versionMinor;
    }
};

struct SchemaIdLess {
    [[nodiscard]] bool operator()(const SchemaId& a, const SchemaId& b) const {
        if (a.kind != b.kind) {
            return a.kind < b.kind;
        }
        if (a.versionMajor != b.versionMajor) {
            return a.versionMajor < b.versionMajor;
        }
        return a.versionMinor < b.versionMinor;
    }
};

class SchemaRegistry {
public:
    void Register(SchemaId id);
    void Register(std::string_view kind, std::uint32_t versionMajor, std::uint32_t versionMinor);
    void RegisterTagged(SchemaId id, std::string_view dispatchTag);
    void RegisterTagged(std::string_view kind,
                        std::uint32_t versionMajor,
                        std::uint32_t versionMinor,
                        std::string_view dispatchTag);
    [[nodiscard]] bool Contains(const SchemaId& id) const;
    [[nodiscard]] bool Contains(std::string_view kind, std::uint32_t versionMajor, std::uint32_t versionMinor) const;
    [[nodiscard]] std::optional<std::string_view> TagFor(const SchemaId& id) const;
    void Clear();

private:
    std::map<SchemaId, std::string, SchemaIdLess> entries_{};
};

} // namespace ri::data::schema
