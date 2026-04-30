#pragma once

#include "RawIron/Scene/Components.h"

#include <optional>
#include <string>
#include <string_view>

namespace ri::scene {

/// Normalized lowercase key for logs / migrations (not necessarily equal to enum string).
[[nodiscard]] std::string CanonicalScenePrimitiveKey(std::string_view token);

/// Prefer \p primaryToken when set; otherwise \p secondaryToken (e.g. JSON `primitiveType` vs `type`).
[[nodiscard]] PrimitiveType ResolveScenePrimitiveTypeFromAuthoring(std::optional<std::string_view> primaryToken,
                                                                   std::optional<std::string_view> secondaryToken,
                                                                   PrimitiveType fallback = PrimitiveType::Cube);

[[nodiscard]] PrimitiveType ResolveScenePrimitiveTypeFromAuthoring(std::string_view singleToken,
                                                                   PrimitiveType fallback = PrimitiveType::Cube);

/// Structural compiler uses different type strings — normalize authoring aliases before \ref ri::structural::BuildPrimitiveMesh.
[[nodiscard]] std::string ResolveStructuralPrimitiveTypeToken(std::optional<std::string_view> primaryToken,
                                                              std::optional<std::string_view> secondaryToken,
                                                              std::string_view fallback = "box");

} // namespace ri::scene
