#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace ri::data::schema {

/// Interchange file header: document kind + schema version (major.minor).
struct DocumentHeader {
    std::string kind;
    std::uint32_t schemaMajor = 0;
    std::uint32_t schemaMinor = 0;
};

/// Parses `major` or `major.minor` (extra segments ignored). Empty version → 0.0.
[[nodiscard]] ri::validate::SafeParseResult<DocumentHeader> ParseDocumentHeader(std::string_view kind,
                                                                                std::string_view versionDotSeparated);

} // namespace ri::data::schema
