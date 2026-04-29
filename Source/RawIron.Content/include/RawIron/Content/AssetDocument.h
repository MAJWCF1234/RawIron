#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

struct AssetReference {
    std::string kind{};
    std::string id{};
    std::string path{};
};

struct AssetDocument {
    static constexpr int kFormatVersion = 1;

    int formatVersion = kFormatVersion;
    std::string id{};
    std::string type{};
    std::string displayName{};
    std::string sourcePath{};
    std::vector<AssetReference> references{};
    std::string payloadJson = "{}";
};

[[nodiscard]] std::string SerializeAssetDocument(const AssetDocument& document);
[[nodiscard]] std::optional<AssetDocument> ParseAssetDocument(std::string_view jsonText);
[[nodiscard]] std::optional<AssetDocument> LoadAssetDocument(const std::filesystem::path& path);
[[nodiscard]] bool SaveAssetDocument(const std::filesystem::path& path, const AssetDocument& document);

} // namespace ri::content

