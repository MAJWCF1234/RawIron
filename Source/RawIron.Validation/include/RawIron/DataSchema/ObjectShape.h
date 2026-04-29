#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace ri::data::schema {

enum class UnknownKeyPolicy : std::uint8_t {
    Forbid = 0,
    AllowExtra = 1,
    PassthroughBag = 2,
    /// Unknown keys are accepted by \ref ValidateObjectShape (like \ref AllowExtra); pair with
    /// `StripUnknownKeysFromValueObject` when normalizing toward a declared interchange surface.
    Strip = 3,
};

/// Optional introspection for forms / docs; not used by \ref ValidateObjectShape.
struct ObjectFieldDoc {
    std::string key;
    bool required = true;
    std::uint32_t optionalSinceMajor = 0;
    std::uint32_t optionalSinceMinor = 0;
    std::string description;
};

/// Declarative object key layout (structural contract). Does not walk a DOM — caller supplies present keys.
struct ObjectShape {
    std::vector<std::string> requiredKeys;
    std::vector<std::string> optionalKeys;
    UnknownKeyPolicy unknownPolicy = UnknownKeyPolicy::Forbid;
    std::string extensionsBagKey;
    std::vector<ObjectFieldDoc> fieldDocs;
};

[[nodiscard]] ri::validate::ValidationReport ValidateObjectShape(const ObjectShape& shape,
                                                                 const std::unordered_set<std::string>& presentKeys);

/// Keys that belong to the declared interchange: required, optional, and (for \ref PassthroughBag) `extensionsBagKey`.
inline void CollectDeclaredObjectKeys(const ObjectShape& shape, std::unordered_set<std::string>& out) {
    for (const std::string& key : shape.requiredKeys) {
        out.insert(key);
    }
    for (const std::string& key : shape.optionalKeys) {
        out.insert(key);
    }
    if (shape.unknownPolicy == UnknownKeyPolicy::PassthroughBag && !shape.extensionsBagKey.empty()) {
        out.insert(shape.extensionsBagKey);
    }
}

} // namespace ri::data::schema
