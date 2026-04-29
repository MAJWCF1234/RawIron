#include "RawIron/Content/PrefabExpansion.h"

#include "RawIron/Math/Mat4.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>

namespace ri::content {

namespace {

constexpr std::array<const char*, 9> kEntityCollections = {
    "geometry",
    "modelInstances",
    "spawners",
    "lights",
    "volumes",
    "interactives",
    "infoPanels",
    "doors",
    "objectives",
};
constexpr float kRadiansToDegrees = 57.29577951308232f;
constexpr float kMinimumScaleMagnitude = 0.0001f;
constexpr float kMaximumScaleMagnitude = 512.0f;

std::optional<double> ParseNumber(const Value& value) {
    if (const double* number = value.TryGetNumber(); number != nullptr) {
        return std::isfinite(*number) ? std::optional<double>(*number) : std::nullopt;
    }

    const std::string* text = value.TryGetString();
    if (text == nullptr || text->empty()) {
        return std::nullopt;
    }

    double parsed = 0.0;
    const auto result = std::from_chars(text->data(), text->data() + text->size(), parsed);
    if (result.ec != std::errc()) {
        return std::nullopt;
    }
    return std::isfinite(parsed) ? std::optional<double>(parsed) : std::nullopt;
}

std::string_view GetStringOrEmpty(const Value* value) {
    if (value == nullptr) {
        return {};
    }
    const std::string* stringValue = value->TryGetString();
    return stringValue == nullptr ? std::string_view{} : std::string_view(*stringValue);
}

bool HasNonZeroRotation(const ri::math::Vec3& rotationRadians) {
    return std::fabs(rotationRadians.x) > 0.0001f ||
           std::fabs(rotationRadians.y) > 0.0001f ||
           std::fabs(rotationRadians.z) > 0.0001f;
}

ri::math::Vec3 ToDegrees(const ri::math::Vec3& rotationRadians) {
    return rotationRadians * kRadiansToDegrees;
}

Value MakeVec3Value(const ri::math::Vec3& value) {
    return Value::Array{
        Value(static_cast<double>(value.x)),
        Value(static_cast<double>(value.y)),
        Value(static_cast<double>(value.z)),
    };
}

ri::math::Vec3 SanitizeVec3FromArray(const Value::Array* array, const ri::math::Vec3& fallback) {
    if (array == nullptr || array->size() < 3U) {
        return fallback;
    }

    const auto resolveComponent = [&](std::size_t index, float fallbackValue) {
        const std::optional<double> parsed = ParseNumber(array->at(index));
        return parsed.has_value() ? static_cast<float>(*parsed) : fallbackValue;
    };

    return ri::math::Vec3{
        resolveComponent(0, fallback.x),
        resolveComponent(1, fallback.y),
        resolveComponent(2, fallback.z),
    };
}

Value MergeTemplateDataInternal(const Value* baseValue, const Value* overrideValue) {
    if (overrideValue == nullptr) {
        return baseValue == nullptr ? Value{} : *baseValue;
    }
    if (baseValue == nullptr) {
        return *overrideValue;
    }

    if (baseValue->IsArray() || overrideValue->IsArray()) {
        return *overrideValue;
    }

    if (baseValue->IsObject() && overrideValue->IsObject()) {
        Value::Object merged = *baseValue->TryGetObject();
        for (const auto& [key, value] : *overrideValue->TryGetObject()) {
            merged[key] = MergeTemplateDataInternal(baseValue->Find(key), &value);
        }
        return Value(std::move(merged));
    }

    return *overrideValue;
}

Value ResolveLevelEntityTemplate(const std::string& templateName,
                                 const Value::Object& templateMap,
                                 std::map<std::string, Value, std::less<>>& cache,
                                 std::set<std::string, std::less<>>& stack) {
    if (templateName.empty()) {
        return Value::Object{};
    }

    if (const auto cached = cache.find(templateName); cached != cache.end()) {
        return cached->second;
    }

    const auto templateIt = templateMap.find(templateName);
    if (templateIt == templateMap.end() || !templateIt->second.IsObject()) {
        throw std::runtime_error("Unknown entity template \"" + templateName + "\".");
    }
    if (stack.contains(templateName)) {
        throw std::runtime_error("Recursive entity template reference detected for \"" + templateName + "\".");
    }

    stack.insert(templateName);
    Value resolved = Value::Object{};
    const Value::Object& templateObject = *templateIt->second.TryGetObject();
    if (const std::string_view parentTemplate = GetStringOrEmpty(templateObject.contains("template") ? &templateObject.at("template") : nullptr);
        !parentTemplate.empty()) {
        resolved = ResolveLevelEntityTemplate(std::string(parentTemplate), templateMap, cache, stack);
    }

    Value::Object localTemplate = templateObject;
    localTemplate.erase("template");
    Value localTemplateValue(std::move(localTemplate));
    resolved = MergeTemplateDataInternal(&resolved, &localTemplateValue);
    stack.erase(templateName);
    cache[templateName] = resolved;
    return resolved;
}

Value ResolveTemplateNode(const Value& node,
                          const Value::Object& templateMap,
                          std::map<std::string, Value, std::less<>>& cache) {
    if (!node.IsObject()) {
        return node;
    }

    const std::string_view templateName = GetStringOrEmpty(node.Find("template"));
    if (templateName.empty()) {
        return node;
    }

    std::set<std::string, std::less<>> stack;
    Value resolvedTemplate = ResolveLevelEntityTemplate(std::string(templateName), templateMap, cache, stack);
    Value merged = MergeTemplateDataInternal(&resolvedTemplate, &node);
    if (Value::Object* mergedObject = merged.TryGetObject(); mergedObject != nullptr) {
        mergedObject->erase("template");
    }
    return merged;
}

void ResolveTemplateNodesInCollections(Value::Object& levelObject,
                                       const Value::Object& templateMap,
                                       std::map<std::string, Value, std::less<>>& cache) {
    for (const char* key : kEntityCollections) {
        Value* collectionValue = levelObject.contains(key) ? &levelObject.at(key) : nullptr;
        Value::Array* collection = collectionValue == nullptr ? nullptr : collectionValue->TryGetArray();
        if (collection == nullptr) {
            continue;
        }

        for (Value& node : *collection) {
            if (!node.IsObject()) {
                throw std::runtime_error(std::string("Template collection \"") + key + "\" contains a non-object node.");
            }
            node = ResolveTemplateNode(node, templateMap, cache);
        }
    }
}

Value::Array& EnsureArray(Value::Object& object, std::string_view key) {
    Value& value = object[std::string(key)];
    if (!value.IsArray()) {
        value = Value::Array{};
    }
    return *value.TryGetArray();
}

void AppendAll(Value::Array& destination, const Value::Array& source) {
    destination.insert(destination.end(), source.begin(), source.end());
}

Value TransformVectorValue(const Value& vectorLike,
                           const ContentTransform& transform,
                           bool rotate,
                           bool scale) {
    const ri::math::Vec3 vector = SanitizeVec3(vectorLike, {0.0f, 0.0f, 0.0f});
    const ri::math::Vec3 effectiveScale = scale ? transform.scale : ri::math::Vec3{1.0f, 1.0f, 1.0f};
    const ri::math::Vec3 effectiveRotation = rotate ? transform.rotationRadians : ri::math::Vec3{0.0f, 0.0f, 0.0f};
    const ri::math::Mat4 matrix = ri::math::TRS({0.0f, 0.0f, 0.0f}, ToDegrees(effectiveRotation), effectiveScale);
    const ri::math::Vec3 transformed = ri::math::TransformPoint(matrix, vector) + transform.position;
    return MakeVec3Value(transformed);
}

struct ExpandedPrefabCollections {
    Value::Array geometry;
    Value::Array modelInstances;
    Value::Array spawners;
    Value::Array lights;
    Value::Array volumes;
    Value::Array interactives;
    Value::Array infoPanels;
    Value::Array doors;
    Value::Array objectives;
};

ExpandedPrefabCollections InstantiateLevelPrefab(const std::string& prefabName,
                                                 const Value::Object& instanceData,
                                                 const Value::Object& prefabMap,
                                                 std::set<std::string, std::less<>>& stack);

ExpandedPrefabCollections AppendPrefabNodes(const Value::Object& prefab,
                                           const ContentTransform& transform,
                                           std::string_view idPrefix) {
    ExpandedPrefabCollections expanded{};

    const auto pushNodes = [&](Value::Array& destination, std::string_view key) {
        const std::string keyString(key);
        const Value* collectionValue = prefab.contains(keyString) ? &prefab.at(keyString) : nullptr;
        const Value::Array* collection = collectionValue == nullptr ? nullptr : collectionValue->TryGetArray();
        if (collection == nullptr) {
            return;
        }

        for (const Value& node : *collection) {
            destination.push_back(TransformPrefabNode(node, transform, idPrefix));
        }
    };

    pushNodes(expanded.geometry, "geometry");
    pushNodes(expanded.modelInstances, "modelInstances");
    pushNodes(expanded.spawners, "spawners");
    pushNodes(expanded.lights, "lights");
    pushNodes(expanded.volumes, "volumes");
    pushNodes(expanded.interactives, "interactives");
    pushNodes(expanded.infoPanels, "infoPanels");
    pushNodes(expanded.doors, "doors");
    pushNodes(expanded.objectives, "objectives");
    return expanded;
}

ExpandedPrefabCollections InstantiateLevelPrefab(const std::string& prefabName,
                                                 const Value::Object& instanceData,
                                                 const Value::Object& prefabMap,
                                                 std::set<std::string, std::less<>>& stack) {
    const auto prefabIt = prefabMap.find(prefabName);
    if (prefabIt == prefabMap.end() || !prefabIt->second.IsObject()) {
        throw std::runtime_error("Unknown prefab \"" + prefabName + "\".");
    }
    if (stack.contains(prefabName)) {
        throw std::runtime_error("Recursive prefab reference detected for \"" + prefabName + "\".");
    }

    stack.insert(prefabName);
    const Value::Object& prefab = *prefabIt->second.TryGetObject();
    const ContentTransform transform = GetPrefabTransform(instanceData);
    const std::string idPrefix(GetStringOrEmpty(instanceData.contains("idPrefix") ? &instanceData.at("idPrefix") : nullptr));
    ExpandedPrefabCollections expanded = AppendPrefabNodes(prefab, transform, idPrefix);

    const Value* prefabInstancesValue = prefab.contains("prefabInstances") ? &prefab.at("prefabInstances") : nullptr;
    const Value::Array* prefabInstances = prefabInstancesValue == nullptr ? nullptr : prefabInstancesValue->TryGetArray();
    if (prefabInstances != nullptr) {
        for (std::size_t index = 0; index < prefabInstances->size(); ++index) {
            const Value& childInstance = prefabInstances->at(index);
            const Value::Object* childObject = childInstance.TryGetObject();
            if (childObject == nullptr) {
                throw std::runtime_error("Prefab \"" + prefabName + "\" contains a non-object prefabInstances entry.");
            }

            const std::string childPrefab(GetStringOrEmpty(childObject->contains("prefab") ? &childObject->at("prefab") : nullptr));
            if (childPrefab.empty()) {
                throw std::runtime_error("Prefab \"" + prefabName + "\" contains a prefabInstances entry without a prefab id.");
            }

            Value::Object mergedChild = *childObject;
            if (!mergedChild.contains("idPrefix") || !mergedChild.at("idPrefix").IsString()) {
                mergedChild["idPrefix"] = idPrefix + prefabName + "_" + std::to_string(index) + "_";
            }

            const Value transformedChild = TransformPrefabNode(Value(mergedChild), transform, {});
            const Value::Object* transformedChildObject = transformedChild.TryGetObject();
            if (transformedChildObject == nullptr) {
                continue;
            }

            ExpandedPrefabCollections nested = InstantiateLevelPrefab(childPrefab, *transformedChildObject, prefabMap, stack);
            AppendAll(expanded.geometry, nested.geometry);
            AppendAll(expanded.modelInstances, nested.modelInstances);
            AppendAll(expanded.spawners, nested.spawners);
            AppendAll(expanded.lights, nested.lights);
            AppendAll(expanded.volumes, nested.volumes);
            AppendAll(expanded.interactives, nested.interactives);
            AppendAll(expanded.infoPanels, nested.infoPanels);
            AppendAll(expanded.doors, nested.doors);
            AppendAll(expanded.objectives, nested.objectives);
        }
    }

    stack.erase(prefabName);
    return expanded;
}

} // namespace

ri::math::Vec3 SanitizeVec3(const Value& value, const ri::math::Vec3& fallback) {
    return SanitizeVec3FromArray(value.TryGetArray(), fallback);
}

std::array<double, 2> SanitizeVec2(const Value& value, const std::array<double, 2>& fallback) {
    const Value::Array* array = value.TryGetArray();
    if (array == nullptr || array->size() < 2U) {
        return fallback;
    }

    const std::optional<double> x = ParseNumber(array->at(0));
    const std::optional<double> y = ParseNumber(array->at(1));
    return {
        x.has_value() ? *x : fallback[0],
        y.has_value() ? *y : fallback[1],
    };
}

std::array<double, 4> SanitizeQuaternion(const Value& value) {
    const Value::Array* array = value.TryGetArray();
    const std::array<double, 4> fallback = {0.0, 0.0, 0.0, 1.0};
    if (array == nullptr || array->size() < 4U) {
        return fallback;
    }

    const std::array<double, 4> raw = {
        ParseNumber(array->at(0)).value_or(0.0),
        ParseNumber(array->at(1)).value_or(0.0),
        ParseNumber(array->at(2)).value_or(0.0),
        ParseNumber(array->at(3)).value_or(1.0),
    };
    const double length = std::sqrt((raw[0] * raw[0]) + (raw[1] * raw[1]) + (raw[2] * raw[2]) + (raw[3] * raw[3]));
    if (!std::isfinite(length) || length < 1e-8) {
        return fallback;
    }

    return {raw[0] / length, raw[1] / length, raw[2] / length, raw[3] / length};
}

ri::math::Vec3 SanitizeScale(const Value& value, const ri::math::Vec3& fallback) {
    const ri::math::Vec3 scale = SanitizeVec3(value, fallback);
    const auto clamp = [](float component) {
        if (!std::isfinite(component) || std::fabs(component) < kMinimumScaleMagnitude) {
            return 1.0f;
        }
        return std::clamp(component, -kMaximumScaleMagnitude, kMaximumScaleMagnitude);
    };

    return ri::math::Vec3{
        clamp(scale.x),
        clamp(scale.y),
        clamp(scale.z),
    };
}

double ClampFiniteNumber(const Value& value, double fallback, double minimum, double maximum) {
    const std::optional<double> parsed = ParseNumber(value);
    if (!parsed.has_value()) {
        return fallback;
    }
    return std::clamp(*parsed, minimum, maximum);
}

int ClampFiniteInteger(const Value& value, int fallback, int minimum, int maximum) {
    return static_cast<int>(std::lround(ClampFiniteNumber(value, static_cast<double>(fallback), minimum, maximum)));
}

double ClampPickupMotion(const Value& value, double fallback, double minimum, double maximum) {
    return ClampFiniteNumber(value, fallback, minimum, maximum);
}

Value MergeTemplateData(const Value& baseValue, const Value& overrideValue) {
    return MergeTemplateDataInternal(&baseValue, &overrideValue);
}

Value ApplyLevelEntityTemplates(const Value& levelData) {
    if (!levelData.IsObject()) {
        return levelData;
    }

    Value expandedLevel = levelData;
    Value::Object& expandedObject = *expandedLevel.TryGetObject();
    const Value* entityTemplatesValue = expandedObject.contains("entityTemplates") ? &expandedObject.at("entityTemplates") : nullptr;
    const Value::Object* entityTemplates = entityTemplatesValue == nullptr ? nullptr : entityTemplatesValue->TryGetObject();
    if (entityTemplates == nullptr || entityTemplates->empty()) {
        return expandedLevel;
    }

    Value::Object templateMap{};
    for (const auto& [key, value] : *entityTemplates) {
        if (value.IsObject()) {
            templateMap[key] = value;
        }
    }
    if (templateMap.empty()) {
        return expandedLevel;
    }

    std::map<std::string, Value, std::less<>> cache;
    ResolveTemplateNodesInCollections(expandedObject, templateMap, cache);

    Value* prefabsValue = expandedObject.contains("prefabs") ? &expandedObject.at("prefabs") : nullptr;
    Value::Object* prefabs = prefabsValue == nullptr ? nullptr : prefabsValue->TryGetObject();
    if (prefabs != nullptr) {
        for (auto& [_, prefabValue] : *prefabs) {
            Value::Object* prefab = prefabValue.TryGetObject();
            if (prefab == nullptr) {
                continue;
            }
            ResolveTemplateNodesInCollections(*prefab, templateMap, cache);
        }
    }

    return expandedLevel;
}

ContentTransform GetPrefabTransform(const Value::Object& transformData) {
    return ContentTransform{
        .position = SanitizeVec3(transformData.contains("position") ? transformData.at("position") : Value{}, {0.0f, 0.0f, 0.0f}),
        .rotationRadians = SanitizeVec3(transformData.contains("rotation") ? transformData.at("rotation") : Value{}, {0.0f, 0.0f, 0.0f}),
        .scale = SanitizeScale(transformData.contains("scale") ? transformData.at("scale") : Value{}, {1.0f, 1.0f, 1.0f}),
    };
}

Value TransformPrefabNode(const Value& node, const ContentTransform& transform, std::string_view idPrefix) {
    Value out = node;
    Value::Object* object = out.TryGetObject();
    if (object == nullptr) {
        return out;
    }

    if (!idPrefix.empty()) {
        Value* id = object->contains("id") ? &object->at("id") : nullptr;
        if (id != nullptr && id->IsString()) {
            *id = std::string(idPrefix) + *id->TryGetString();
        }
    }

    if (Value* position = object->contains("position") ? &object->at("position") : nullptr; position != nullptr && position->IsArray()) {
        *position = TransformVectorValue(*position, transform, true, true);
    }

    if (Value* rotation = object->contains("rotation") ? &object->at("rotation") : nullptr; rotation != nullptr && rotation->IsArray()) {
        const ri::math::Vec3 base = SanitizeVec3(*rotation, {0.0f, 0.0f, 0.0f});
        *rotation = MakeVec3Value(base + transform.rotationRadians);
    } else if (HasNonZeroRotation(transform.rotationRadians)) {
        (*object)["rotation"] = MakeVec3Value(transform.rotationRadians);
    }

    if (Value* scale = object->contains("scale") ? &object->at("scale") : nullptr; scale != nullptr && scale->IsArray()) {
        const ri::math::Vec3 baseScale = SanitizeVec3(*scale, {1.0f, 1.0f, 1.0f});
        *scale = MakeVec3Value(ri::math::Vec3{
            baseScale.x * transform.scale.x,
            baseScale.y * transform.scale.y,
            baseScale.z * transform.scale.z,
        });
    }

    if (Value* lookAt = object->contains("lookAt") ? &object->at("lookAt") : nullptr; lookAt != nullptr && lookAt->IsArray()) {
        *lookAt = TransformVectorValue(*lookAt, transform, true, true);
    }

    if (Value* path = object->contains("path") ? &object->at("path") : nullptr; path != nullptr && path->IsArray()) {
        Value::Array transformedPath{};
        for (const Value& point : *path->TryGetArray()) {
            transformedPath.push_back(point.IsArray() ? TransformVectorValue(point, transform, true, true) : point);
        }
        *path = Value(std::move(transformedPath));
    }

    return out;
}

Value ExpandLevelPrefabs(const Value& levelData) {
    if (!levelData.IsObject()) {
        return levelData;
    }

    Value expandedLevel = levelData;
    Value::Object& expandedObject = *expandedLevel.TryGetObject();
    const Value* prefabsValue = expandedObject.contains("prefabs") ? &expandedObject.at("prefabs") : nullptr;
    const Value::Object* prefabs = prefabsValue == nullptr ? nullptr : prefabsValue->TryGetObject();
    const Value* prefabInstancesValue = expandedObject.contains("prefabInstances") ? &expandedObject.at("prefabInstances") : nullptr;
    const Value::Array* prefabInstances = prefabInstancesValue == nullptr ? nullptr : prefabInstancesValue->TryGetArray();

    if (prefabs == nullptr || prefabs->empty() || prefabInstances == nullptr || prefabInstances->empty()) {
        return expandedLevel;
    }

    Value::Object prefabMap{};
    for (const auto& [key, value] : *prefabs) {
        if (value.IsObject()) {
            prefabMap[key] = value;
        }
    }
    if (prefabMap.empty()) {
        return expandedLevel;
    }

    Value::Array& geometry = EnsureArray(expandedObject, "geometry");
    Value::Array& modelInstances = EnsureArray(expandedObject, "modelInstances");
    Value::Array& spawners = EnsureArray(expandedObject, "spawners");
    Value::Array& lights = EnsureArray(expandedObject, "lights");
    Value::Array& volumes = EnsureArray(expandedObject, "volumes");
    Value::Array& interactives = EnsureArray(expandedObject, "interactives");
    Value::Array& infoPanels = EnsureArray(expandedObject, "infoPanels");
    Value::Array& doors = EnsureArray(expandedObject, "doors");
    Value::Array& objectives = EnsureArray(expandedObject, "objectives");

    for (std::size_t index = 0; index < prefabInstances->size(); ++index) {
        const Value& instanceValue = prefabInstances->at(index);
        const Value::Object* instanceObject = instanceValue.TryGetObject();
        if (instanceObject == nullptr) {
            throw std::runtime_error("Level prefabInstances contains a non-object entry.");
        }

        const std::string prefabName(GetStringOrEmpty(instanceObject->contains("prefab") ? &instanceObject->at("prefab") : nullptr));
        if (prefabName.empty()) {
            throw std::runtime_error("Level prefabInstances contains an entry without a prefab id.");
        }

        Value::Object instance = *instanceObject;
        if (!instance.contains("idPrefix") || !instance.at("idPrefix").IsString()) {
            instance["idPrefix"] = prefabName + "_" + std::to_string(index) + "_";
        }

        std::set<std::string, std::less<>> stack;
        const ExpandedPrefabCollections expandedPrefab = InstantiateLevelPrefab(prefabName, instance, prefabMap, stack);
        AppendAll(geometry, expandedPrefab.geometry);
        AppendAll(modelInstances, expandedPrefab.modelInstances);
        AppendAll(spawners, expandedPrefab.spawners);
        AppendAll(lights, expandedPrefab.lights);
        AppendAll(volumes, expandedPrefab.volumes);
        AppendAll(interactives, expandedPrefab.interactives);
        AppendAll(infoPanels, expandedPrefab.infoPanels);
        AppendAll(doors, expandedPrefab.doors);
        AppendAll(objectives, expandedPrefab.objectives);
    }

    return expandedLevel;
}

Value ExpandLevelAuthoringData(const Value& levelData) {
    return ApplyLevelEntityTemplates(ExpandLevelPrefabs(levelData));
}

} // namespace ri::content
