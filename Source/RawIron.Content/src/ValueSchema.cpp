#include "RawIron/Content/ValueSchema.h"

#include "RawIron/DataSchema/CollectionConstraints.h"
#include "RawIron/DataSchema/StringPattern.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ri::content {
namespace {

[[nodiscard]] bool ValueMatchesSlotKind(const Value& element, const ValueSlotKind kind) {
    switch (kind) {
    case ValueSlotKind::Boolean:
        return element.IsBoolean();
    case ValueSlotKind::Number:
        return element.IsNumber();
    case ValueSlotKind::String:
        return element.IsString();
    case ValueSlotKind::Array:
        return element.IsArray();
    case ValueSlotKind::Object:
        return element.IsObject();
    }
    return false;
}

[[nodiscard]] std::string ValueKindLabel(const Value& value) {
    if (value.IsNull()) {
        return "null";
    }
    if (value.IsBoolean()) {
        return "boolean";
    }
    if (value.IsNumber()) {
        return "number";
    }
    if (value.IsString()) {
        return "string";
    }
    if (value.IsArray()) {
        return "array";
    }
    if (value.IsObject()) {
        return "object";
    }
    return "unknown";
}

[[nodiscard]] const char* SlotKindLabel(const ValueSlotKind kind) {
    switch (kind) {
    case ValueSlotKind::Boolean:
        return "boolean";
    case ValueSlotKind::Number:
        return "number";
    case ValueSlotKind::String:
        return "string";
    case ValueSlotKind::Array:
        return "array";
    case ValueSlotKind::Object:
        return "object";
    }
    return "unknown";
}

void AppendDottedAsPathSegments(std::string& out, const std::string_view dotted) {
    std::size_t offset = 0;
    while (offset < dotted.size()) {
        const std::size_t sep = dotted.find('.', offset);
        const std::size_t len =
            sep == std::string_view::npos ? (dotted.size() - offset) : (sep - offset);
        if (len == 0U) {
            return;
        }
        out.push_back('/');
        out.append(dotted.substr(offset, len));
        if (sep == std::string_view::npos) {
            break;
        }
        offset = sep + 1U;
    }
}

[[nodiscard]] std::string PathForUnresolved(const std::string_view pathPrefix, const std::string_view dottedPath) {
    std::string out;
    if (pathPrefix.empty()) {
        out = "/";
    } else {
        out.assign(pathPrefix);
    }
    AppendDottedAsPathSegments(out, dottedPath);
    return out;
}

} // namespace

std::string ValueKindDebugName(const Value& value) {
    return ValueKindLabel(value);
}

ri::validate::ValidationReport ValidateValueObjectShape(const Value& value,
                                                        const ri::data::schema::ObjectShape& shape,
                                                        std::string_view pathPrefix) {
    ri::validate::ValidationReport report;
    const Value::Object* object = value.TryGetObject();
    if (object == nullptr) {
        report.AddTypeMismatch(pathPrefix.empty() ? std::string("/") : std::string(pathPrefix),
                               "object",
                               ValueKindLabel(value));
        return report;
    }
    std::unordered_set<std::string> keys;
    keys.reserve(object->size());
    for (const auto& entry : *object) {
        keys.insert(entry.first);
    }
    const ri::validate::ValidationReport shapeReport = ri::data::schema::ValidateObjectShape(shape, keys);
    report.AbsorbPrefixed(pathPrefix, shapeReport);
    return report;
}

ri::validate::ValidationReport ValidateValueArrayShape(const Value& value,
                                                       const std::size_t minCount,
                                                       const std::size_t maxCount,
                                                       const std::optional<ValueSlotKind> elementKind,
                                                       std::string path) {
    ri::validate::ValidationReport report;
    const Value::Array* array = value.TryGetArray();
    if (array == nullptr) {
        report.AddTypeMismatch(path.empty() ? std::string("/") : path, "array", ValueKindLabel(value));
        return report;
    }
    report.Merge(ri::validate::ValidateCollectionSize(array->size(), minCount, maxCount, path));
    if (!elementKind.has_value()) {
        return report;
    }
    for (std::size_t i = 0; i < array->size(); ++i) {
        const Value& element = (*array)[i];
        if (!ValueMatchesSlotKind(element, *elementKind)) {
            std::string elementPath = path.empty() ? std::string("/") : path;
            if (elementPath.back() != '/') {
                elementPath.push_back('/');
            }
            elementPath += std::to_string(i);
            report.AddTypeMismatch(std::move(elementPath),
                                   SlotKindLabel(*elementKind),
                                   ValueKindLabel(element),
                                   "array element type mismatch");
        }
    }
    return report;
}

ri::validate::ValidationReport ValidateValueArrayTupleShape(const Value& value,
                                                            const std::vector<ValueSlotKind>& slotKinds,
                                                            std::string path) {
    ri::validate::ValidationReport report;
    const Value::Array* array = value.TryGetArray();
    if (array == nullptr) {
        report.AddTypeMismatch(path.empty() ? std::string("/") : path, "array", ValueKindLabel(value));
        return report;
    }
    if (array->size() != slotKinds.size()) {
        report.Add(ri::validate::IssueCode::ConstraintViolation,
                   path.empty() ? std::string("/") : path,
                   "tuple length mismatch");
        return report;
    }
    for (std::size_t i = 0; i < slotKinds.size(); ++i) {
        if (!ValueMatchesSlotKind((*array)[i], slotKinds[i])) {
            std::string elementPath = path.empty() ? std::string("/") : path;
            if (elementPath.back() != '/') {
                elementPath.push_back('/');
            }
            elementPath += std::to_string(i);
            report.AddTypeMismatch(std::move(elementPath),
                                   SlotKindLabel(slotKinds[i]),
                                   ValueKindLabel((*array)[i]),
                                   "tuple element type mismatch");
        }
    }
    return report;
}

ri::validate::ValidationReport ValidateValueObjectShapeAtPath(const Value& root,
                                                              const std::string_view dottedPath,
                                                              const ri::data::schema::ObjectShape& shape,
                                                              const std::string_view pathPrefix) {
    const Value* node = dottedPath.empty() ? &root : root.FindPath(dottedPath);
    ri::validate::ValidationReport report;
    if (node == nullptr) {
        report.Add(ri::validate::IssueCode::MissingField,
                   PathForUnresolved(pathPrefix, dottedPath),
                   "value path did not resolve");
        return report;
    }
    const std::string nestedPrefix =
        dottedPath.empty() ? std::string(pathPrefix) : PathForUnresolved(pathPrefix, dottedPath);
    return ValidateValueObjectShape(*node, shape, nestedPrefix);
}

ri::validate::SafeParseResult<std::size_t> TryValidateValueOrderedUnion(const Value& value,
                                                                       std::string path,
                                                                       const std::span<const ValueUnionBranchValidator> branches) {
    ri::validate::SafeParseResult<std::size_t> out;
    if (branches.empty()) {
        out.report.Add(ri::validate::IssueCode::ConstraintViolation,
                       std::move(path),
                       "ordered union has no branches");
        return out;
    }
    ri::validate::ValidationReport combined;
    for (std::size_t i = 0; i < branches.size(); ++i) {
        std::string branchPath;
        if (path.empty()) {
            branchPath = "/__branch/";
        } else {
            branchPath = path;
            if (branchPath.back() != '/') {
                branchPath.push_back('/');
            }
            branchPath += "__branch/";
        }
        branchPath += std::to_string(i);
        ri::validate::ValidationReport branchReport = branches[i](value, branchPath);
        if (branchReport.Ok()) {
            out.value = i;
            return out;
        }
        combined.Merge(std::move(branchReport));
    }
    out.report = std::move(combined);
    out.report.Add(ri::validate::IssueCode::NoUnionMatch, path.empty() ? std::string("/") : path,
                    "no ordered union branch matched");
    return out;
}

ri::validate::SafeParseResult<std::size_t> TryValidateValueStringDiscriminatedUnion(
    const Value& value,
    std::string path,
    const std::string_view discKey,
    const std::span<const StringDiscriminatedUnionBranch> branches) {
    ri::validate::SafeParseResult<std::size_t> out;
    if (branches.empty()) {
        out.report.Add(ri::validate::IssueCode::ConstraintViolation,
                       path.empty() ? std::string("/") : path,
                       "discriminated union has no branches");
        return out;
    }
    const ri::validate::SafeParseResult<std::string> disc = TryReadDiscriminatorString(value, discKey, path);
    if (!disc.value.has_value()) {
        out.report = std::move(disc.report);
        return out;
    }
    for (std::size_t i = 0; i < branches.size(); ++i) {
        if (branches[i].tag != *disc.value) {
            continue;
        }
        ri::validate::ValidationReport branchReport = branches[i].validate(value, path);
        if (branchReport.Ok()) {
            out.value = i;
            return out;
        }
        out.report = std::move(branchReport);
        return out;
    }
    std::string discPath = path.empty() ? std::string("/") : path;
    if (discPath.back() != '/') {
        discPath.push_back('/');
    }
    discPath.append(discKey);
    out.report.AddWithContext(ri::validate::IssueCode::InvalidEnum,
                              std::move(discPath),
                              "unknown discriminator value",
                              "value",
                              *disc.value);
    return out;
}

ri::validate::ValidationReport ValidateValuePatchObject(const Value& value,
                                                       std::string path,
                                                       const std::span<const PatchFieldSpec> fields) {
    ri::validate::ValidationReport report;
    const Value::Object* object = value.TryGetObject();
    if (object == nullptr) {
        report.AddTypeMismatch(path.empty() ? std::string("/") : path, "object", ValueKindLabel(value));
        return report;
    }
    std::unordered_map<std::string, std::optional<ValueSlotKind>> allowed;
    allowed.reserve(fields.size());
    for (const PatchFieldSpec& spec : fields) {
        allowed.insert_or_assign(spec.key, spec.valueKind);
    }
    for (const auto& entry : *object) {
        const auto it = allowed.find(entry.first);
        if (it == allowed.end()) {
            std::string keyPath = path.empty() ? std::string("/") : path;
            keyPath.push_back('/');
            keyPath += entry.first;
            report.Add(ri::validate::IssueCode::UnknownKey, std::move(keyPath), "patch key not allowed");
            continue;
        }
        if (it->second.has_value()) {
            if (!ValueMatchesSlotKind(entry.second, *it->second)) {
                std::string valuePath = path.empty() ? std::string("/") : path;
                valuePath.push_back('/');
                valuePath += entry.first;
                report.AddTypeMismatch(std::move(valuePath),
                                       SlotKindLabel(*it->second),
                                       ValueKindLabel(entry.second),
                                       "patch value type mismatch");
            }
        }
    }
    return report;
}

ri::validate::ValidationReport ValidateValueExtensionsObjectShape(const Value& root,
                                                                  const std::string_view extensionsKey,
                                                                  const ri::data::schema::ObjectShape& innerShape,
                                                                  const std::string_view pathPrefix) {
    ri::validate::ValidationReport report;
    const Value::Object* object = root.TryGetObject();
    if (object == nullptr) {
        report.AddTypeMismatch(pathPrefix.empty() ? std::string("/") : std::string(pathPrefix),
                               "object",
                               ValueKindLabel(root),
                               "expected object for extensions validation");
        return report;
    }
    const auto it = object->find(extensionsKey);
    if (it == object->end()) {
        return report;
    }
    const Value& bag = it->second;
    std::string innerPrefix(pathPrefix);
    if (!innerPrefix.empty()) {
        innerPrefix.push_back('/');
    }
    innerPrefix.append(extensionsKey);
    return ValidateValueObjectShape(bag, innerShape, innerPrefix);
}

ri::validate::ValidationReport ValidateValueObjectShapeWithExtensions(const Value& value,
                                                                      const ri::data::schema::ObjectShape& topShape,
                                                                      const std::string_view extensionsKey,
                                                                      const ri::data::schema::ObjectShape& extensionsShape,
                                                                      const std::string_view pathPrefix) {
    ri::validate::ValidationReport report = ValidateValueObjectShape(value, topShape, pathPrefix);
    if (extensionsKey.empty()) {
        return report;
    }
    if (value.TryGetObject() == nullptr) {
        return report;
    }
    report.Merge(ValidateValueExtensionsObjectShape(value, extensionsKey, extensionsShape, pathPrefix));
    return report;
}

void StripUnknownKeysFromValueObject(Value::Object& object, const ri::data::schema::ObjectShape& shape) {
    std::unordered_set<std::string> allowed;
    ri::data::schema::CollectDeclaredObjectKeys(shape, allowed);
    for (auto it = object.begin(); it != object.end();) {
        if (allowed.find(it->first) == allowed.end()) {
            it = object.erase(it);
        } else {
            ++it;
        }
    }
}

ri::validate::ValidationReport StripThenValidateValueObjectShape(Value& value,
                                                                const ri::data::schema::ObjectShape& shape,
                                                                const std::string_view pathPrefix) {
    ri::validate::ValidationReport report;
    Value::Object* object = value.TryGetObject();
    if (object == nullptr) {
        report.AddTypeMismatch(pathPrefix.empty() ? std::string("/") : std::string(pathPrefix),
                               "object",
                               ValueKindLabel(value));
        return report;
    }
    StripUnknownKeysFromValueObject(*object, shape);
    return ValidateValueObjectShape(value, shape, pathPrefix);
}

ri::validate::ValidationReport ValidateValueObjectKeyPattern(const Value& value,
                                                              const std::string& pattern,
                                                              std::string objectPath) {
    const Value::Object* object = value.TryGetObject();
    if (object == nullptr) {
        ri::validate::ValidationReport rep;
        rep.AddTypeMismatch(objectPath.empty() ? std::string("/") : objectPath,
                            "object",
                            ValueKindLabel(value));
        return rep;
    }
    std::vector<std::string_view> keys;
    keys.reserve(object->size());
    for (const auto& entry : *object) {
        keys.push_back(entry.first);
    }
    return ri::validate::ValidateEachObjectKeyMatchesPattern(objectPath, keys, pattern);
}

void ApplyValueObjectDefaults(Value::Object& object, const std::vector<std::pair<std::string, Value>>& defaults) {
    for (const auto& entry : defaults) {
        if (object.find(entry.first) == object.end()) {
            object.emplace(entry.first, entry.second);
        }
    }
}

void RenameKeyInValueObject(Value::Object& object, std::string_view fromKey, std::string_view toKey) {
    if (fromKey.empty() || toKey.empty() || fromKey == toKey) {
        return;
    }
    const auto fromIt = object.find(fromKey);
    if (fromIt == object.end()) {
        return;
    }
    if (object.find(toKey) != object.end()) {
        return;
    }
    Value moved = std::move(fromIt->second);
    object.erase(fromIt);
    object.emplace(std::string(toKey), std::move(moved));
}

ri::validate::SafeParseResult<std::string> TryReadDiscriminatorString(const Value& value,
                                                                    std::string_view discKey,
                                                                    std::string path) {
    ri::validate::SafeParseResult<std::string> out;
    const Value::Object* object = value.TryGetObject();
    if (object == nullptr) {
        out.report.AddTypeMismatch(path.empty() ? std::string("/") : path,
                                   "object",
                                   ValueKindLabel(value),
                                   "expected object for discriminated union");
        return out;
    }
    const auto it = object->find(discKey);
    if (it == object->end()) {
        out.report.Add(ri::validate::IssueCode::MissingField,
                       path + '/' + std::string(discKey),
                       "missing discriminator field");
        return out;
    }
    const std::string* text = it->second.TryGetString();
    if (text == nullptr) {
        out.report.AddTypeMismatch(path + '/' + std::string(discKey),
                                   "string",
                                   ValueKindLabel(it->second),
                                   "discriminator must be a string");
        return out;
    }
    out.value = *text;
    return out;
}

} // namespace ri::content
