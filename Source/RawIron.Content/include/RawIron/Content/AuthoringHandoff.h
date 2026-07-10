#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

enum class AuthoringAssetKind {
    ModelSource,
    Rig,
    Unknown,
};

struct AuthoringHandoffRequest {
    std::filesystem::path workspaceRoot;
    std::filesystem::path assetPath;
    std::string gameId{};
};

struct AuthoringHandoffReport {
    bool valid = false;
    AuthoringAssetKind assetKind = AuthoringAssetKind::Unknown;
    std::filesystem::path workspaceRoot;
    std::filesystem::path assetPath;
    std::filesystem::path workspaceRelativePath;
    std::vector<std::string> editorArguments;
    std::vector<std::string> issues;
};

[[nodiscard]] std::string_view ToString(AuthoringAssetKind kind) noexcept;

/// Validates a Forge/editor asset handoff and builds the canonical RawIron.Editor argument list.
[[nodiscard]] AuthoringHandoffReport BuildAuthoringHandoff(const AuthoringHandoffRequest& request);

} // namespace ri::content
