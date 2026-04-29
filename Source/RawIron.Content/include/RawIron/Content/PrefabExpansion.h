#pragma once

#include "RawIron/Content/Value.h"
#include "RawIron/Math/Vec3.h"

#include <array>
#include <string_view>

namespace ri::content {

struct ContentTransform {
    ri::math::Vec3 position{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 rotationRadians{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 scale{1.0f, 1.0f, 1.0f};
};

ri::math::Vec3 SanitizeVec3(const Value& value, const ri::math::Vec3& fallback = {0.0f, 0.0f, 0.0f});
std::array<double, 2> SanitizeVec2(const Value& value, const std::array<double, 2>& fallback = {1.0, 1.0});
std::array<double, 4> SanitizeQuaternion(const Value& value);
ri::math::Vec3 SanitizeScale(const Value& value, const ri::math::Vec3& fallback = {1.0f, 1.0f, 1.0f});
double ClampFiniteNumber(const Value& value, double fallback, double minimum, double maximum);
int ClampFiniteInteger(const Value& value, int fallback, int minimum, int maximum);
double ClampPickupMotion(const Value& value, double fallback, double minimum, double maximum);

Value MergeTemplateData(const Value& baseValue, const Value& overrideValue);
Value ApplyLevelEntityTemplates(const Value& levelData);

ContentTransform GetPrefabTransform(const Value::Object& transformData);
Value TransformPrefabNode(const Value& node, const ContentTransform& transform, std::string_view idPrefix = {});
Value ExpandLevelPrefabs(const Value& levelData);
Value ExpandLevelAuthoringData(const Value& levelData);

} // namespace ri::content
