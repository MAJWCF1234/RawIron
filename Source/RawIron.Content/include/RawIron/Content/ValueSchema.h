#pragma once

#include "RawIron/Content/Value.h"
#include "RawIron/DataSchema/ObjectShape.h"
#include "RawIron/DataSchema/ValidationReport.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ri::content {

enum class ValueSlotKind : std::uint8_t {
    Boolean = 0,
    Number = 1,
    String = 2,
    Array = 3,
    Object = 4,
};

/// One branch of an untagged (ordered) union: return an empty report if `value` matches this branch.
using ValueUnionBranchValidator = ri::validate::ValidationReport (*)(const Value& value, std::string path);

/// String-discriminated union: `tag` matches the string at `discKey` on the object; `validate` checks payload shape.
struct StringDiscriminatedUnionBranch {
    std::string_view tag;
    ValueUnionBranchValidator validate;
};

/// Stable JSON-ish kind label for `value` (`null`, `boolean`, `number`, …) for tools and error hints.
[[nodiscard]] std::string ValueKindDebugName(const Value& value);

/// Allowed keys for a partial document / patch. Optional `valueKind` skips value typing for that key.
struct PatchFieldSpec {
    std::string key;
    std::optional<ValueSlotKind> valueKind;
};

/// Structural check: `value` must be an object; keys are checked with \ref ri::data::schema::ValidateObjectShape.
/// Use `pathPrefix` without a trailing `/` (or empty for root); issue paths from shape are joined under it.
[[nodiscard]] ri::validate::ValidationReport ValidateValueObjectShape(
    const Value& value,
    const ri::data::schema::ObjectShape& shape,
    std::string_view pathPrefix = {});

/// Structural check: `value` must be an array; optional per-element kind and inclusive size bounds.
[[nodiscard]] ri::validate::ValidationReport ValidateValueArrayShape(const Value& value,
                                                                     std::size_t minCount,
                                                                     std::size_t maxCount,
                                                                     std::optional<ValueSlotKind> elementKind,
                                                                     std::string path);

/// Fixed-length heterogeneous array: `slotKinds[i]` constrains element `i`. Empty `slotKinds` requires an empty array.
[[nodiscard]] ri::validate::ValidationReport ValidateValueArrayTupleShape(const Value& value,
                                                                           const std::vector<ValueSlotKind>& slotKinds,
                                                                           std::string path);

/// Validates `root.FindPath(dottedPath)` as an object against `shape`. Missing path → \ref ri::validate::IssueCode::MissingField.
[[nodiscard]] ri::validate::ValidationReport ValidateValueObjectShapeAtPath(const Value& root,
                                                                            std::string_view dottedPath,
                                                                            const ri::data::schema::ObjectShape& shape,
                                                                            std::string_view pathPrefix = {});

/// If `extensionsKey` is present on `root` (object), it must be an object validated with `innerShape`. Absent key is ok.
[[nodiscard]] ri::validate::ValidationReport ValidateValueExtensionsObjectShape(
    const Value& root,
    std::string_view extensionsKey,
    const ri::data::schema::ObjectShape& innerShape,
    std::string_view pathPrefix = {});

/// Runs \ref ValidateValueObjectShape then optional nested extensions object validation.
[[nodiscard]] ri::validate::ValidationReport ValidateValueObjectShapeWithExtensions(
    const Value& value,
    const ri::data::schema::ObjectShape& topShape,
    std::string_view extensionsKey,
    const ri::data::schema::ObjectShape& extensionsShape,
    std::string_view pathPrefix = {});

/// Removes keys not in \ref ri::data::schema::CollectDeclaredObjectKeys (normalization toward declared surface).
void StripUnknownKeysFromValueObject(Value::Object& object, const ri::data::schema::ObjectShape& shape);

/// Strips unknown keys on the object root then runs \ref ValidateValueObjectShape. `value` must hold an object.
[[nodiscard]] ri::validate::ValidationReport StripThenValidateValueObjectShape(
    Value& value,
    const ri::data::schema::ObjectShape& shape,
    std::string_view pathPrefix = {});

/// Every object property name must match `pattern` (ECMA regex). `objectPath` is the logical path to the object.
[[nodiscard]] ri::validate::ValidationReport ValidateValueObjectKeyPattern(const Value& value,
                                                                           const std::string& pattern,
                                                                           std::string objectPath);

/// Tries each branch in order; first empty report wins. Each branch receives a path ending in `/__branch/{i}`
/// under `path` so failures stay separated. On total failure, merges branch reports and adds
/// \ref ri::validate::IssueCode::NoUnionMatch.
[[nodiscard]] ri::validate::SafeParseResult<std::size_t> TryValidateValueOrderedUnion(
    const Value& value,
    std::string path,
    std::span<const ValueUnionBranchValidator> branches);

/// Reads `discKey`, finds a matching `tag`, then runs that branch validator. Unknown tag → \ref
/// ri::validate::IssueCode::InvalidEnum. Matching tag with a failing branch returns that branch's report only.
[[nodiscard]] ri::validate::SafeParseResult<std::size_t> TryValidateValueStringDiscriminatedUnion(
    const Value& value,
    std::string path,
    std::string_view discKey,
    std::span<const StringDiscriminatedUnionBranch> branches);

/// Validates a patch object: only `fields` keys allowed; optional per-key \ref ValueSlotKind checks.
[[nodiscard]] ri::validate::ValidationReport ValidateValuePatchObject(const Value& value,
                                                                      std::string path,
                                                                      std::span<const PatchFieldSpec> fields);

/// Adds default entries only when the key is absent (deterministic normalization).
void ApplyValueObjectDefaults(Value::Object& object, const std::vector<std::pair<std::string, Value>>& defaults);

/// In-place migration helper: moves `fromKey` to `toKey` if present and `toKey` is unused.
void RenameKeyInValueObject(Value::Object& object, std::string_view fromKey, std::string_view toKey);

/// Discriminated-union helper: read string discriminator field from an object value.
[[nodiscard]] ri::validate::SafeParseResult<std::string> TryReadDiscriminatorString(const Value& value,
                                                                                    std::string_view discKey,
                                                                                    std::string path);

} // namespace ri::content
