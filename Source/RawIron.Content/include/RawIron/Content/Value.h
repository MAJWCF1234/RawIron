#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ri::content {

struct Value {
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value, std::less<>>;
    using Storage = std::variant<std::monostate, bool, double, std::string, Array, Object>;

    Storage storage{};

    Value() = default;
    Value(std::nullptr_t) : storage(std::monostate{}) {}
    Value(bool value) : storage(value) {}
    Value(int value) : storage(static_cast<double>(value)) {}
    Value(double value) : storage(value) {}
    Value(const char* value) : storage(std::string(value == nullptr ? "" : value)) {}
    Value(std::string value) : storage(std::move(value)) {}
    Value(Array value) : storage(std::move(value)) {}
    Value(Object value) : storage(std::move(value)) {}

    [[nodiscard]] bool IsNull() const noexcept { return std::holds_alternative<std::monostate>(storage); }
    [[nodiscard]] bool IsBoolean() const noexcept { return std::holds_alternative<bool>(storage); }
    [[nodiscard]] bool IsNumber() const noexcept { return std::holds_alternative<double>(storage); }
    [[nodiscard]] bool IsString() const noexcept { return std::holds_alternative<std::string>(storage); }
    [[nodiscard]] bool IsArray() const noexcept { return std::holds_alternative<Array>(storage); }
    [[nodiscard]] bool IsObject() const noexcept { return std::holds_alternative<Object>(storage); }

    [[nodiscard]] const bool* TryGetBoolean() const noexcept { return std::get_if<bool>(&storage); }
    [[nodiscard]] const double* TryGetNumber() const noexcept { return std::get_if<double>(&storage); }
    [[nodiscard]] const std::string* TryGetString() const noexcept { return std::get_if<std::string>(&storage); }
    [[nodiscard]] const Array* TryGetArray() const noexcept { return std::get_if<Array>(&storage); }
    [[nodiscard]] const Object* TryGetObject() const noexcept { return std::get_if<Object>(&storage); }

    [[nodiscard]] bool* TryGetBoolean() noexcept { return std::get_if<bool>(&storage); }
    [[nodiscard]] double* TryGetNumber() noexcept { return std::get_if<double>(&storage); }
    [[nodiscard]] std::string* TryGetString() noexcept { return std::get_if<std::string>(&storage); }
    [[nodiscard]] Array* TryGetArray() noexcept { return std::get_if<Array>(&storage); }
    [[nodiscard]] Object* TryGetObject() noexcept { return std::get_if<Object>(&storage); }

    [[nodiscard]] const Value* Find(std::string_view key) const noexcept {
        const Object* object = TryGetObject();
        if (object == nullptr) {
            return nullptr;
        }

        const auto it = object->find(key);
        return it == object->end() ? nullptr : &it->second;
    }

    [[nodiscard]] Value* Find(std::string_view key) noexcept {
        Object* object = TryGetObject();
        if (object == nullptr) {
            return nullptr;
        }

        const auto it = object->find(key);
        return it == object->end() ? nullptr : &it->second;
    }

    /// Resolves a dotted object path (e.g. "player.tuning.walkSpeed").
    [[nodiscard]] const Value* FindPath(std::string_view dottedPath) const noexcept;
    [[nodiscard]] Value* FindPath(std::string_view dottedPath) noexcept;

    /// Coerces common dynamic forms into a number:
    /// - number -> itself
    /// - bool -> 1 or 0
    /// - string -> parsed finite float
    [[nodiscard]] std::optional<double> CoerceNumber() const noexcept;

    /// Coerces common dynamic forms into a boolean:
    /// - bool -> itself
    /// - number -> (value != 0)
    /// - string -> true/false/yes/no/on/off/1/0 (case-insensitive)
    [[nodiscard]] std::optional<bool> CoerceBoolean() const noexcept;

    /// Parses a loose scalar token used by script/debug entry points.
    /// Recognizes null/bool/finite-number; otherwise returns string.
    [[nodiscard]] static Value ParseLooseScalar(std::string_view text);
};

} // namespace ri::content
