#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

enum class ExtensionKind {
    Mod,
    Plugin,
    DataPack,
    Pipe,
};

enum class ExtensionScope {
    Game,
    Editor,
    Engine,
    Shared,
};

enum class ExtensionHost {
    Runtime,
    Editor,
    External,
    Hybrid,
};

struct ExtensionDescriptor {
    static constexpr int kFormatVersion = 1;

    int formatVersion = kFormatVersion;
    std::string id{};
    std::string displayName{};
    std::string version{"0.1.0"};
    ExtensionKind kind = ExtensionKind::Plugin;
    ExtensionScope scope = ExtensionScope::Shared;
    ExtensionHost host = ExtensionHost::Runtime;
    std::string entry{};
    std::string description{};
    std::vector<std::string> capabilities{};
    std::vector<std::string> tags{};
};

struct ExtensionValidationReport {
    bool valid = false;
    std::vector<std::string> issues{};
};

[[nodiscard]] std::optional<ExtensionKind> ParseExtensionKind(std::string_view value);
[[nodiscard]] std::optional<ExtensionScope> ParseExtensionScope(std::string_view value);
[[nodiscard]] std::optional<ExtensionHost> ParseExtensionHost(std::string_view value);

[[nodiscard]] std::string_view ToString(ExtensionKind value) noexcept;
[[nodiscard]] std::string_view ToString(ExtensionScope value) noexcept;
[[nodiscard]] std::string_view ToString(ExtensionHost value) noexcept;

[[nodiscard]] std::string SerializeExtensionDescriptor(const ExtensionDescriptor& descriptor);
[[nodiscard]] std::optional<ExtensionDescriptor> ParseExtensionDescriptor(std::string_view jsonText);
[[nodiscard]] std::optional<ExtensionDescriptor> ExtractExtensionDescriptor(std::string_view jsonText,
                                                                            std::string_view key = "extension");
[[nodiscard]] ExtensionValidationReport ValidateExtensionDescriptor(const ExtensionDescriptor& descriptor);

} // namespace ri::content
