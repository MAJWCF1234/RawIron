#pragma once

#include "EditorWorkspace.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ri::editor {

struct ResourceDocumentData {
    std::filesystem::path absolutePath;
    std::string utf8;
    std::string auxMessage;
    std::vector<std::string> manifestIssues;
    bool isTextEditable = false;
};

[[nodiscard]] ResourceDocumentData LoadResourceDocument(const WorkspaceResourceEntry& entry);
[[nodiscard]] bool SaveResourceDocumentUtf8(const std::filesystem::path& absolutePath, const std::string& utf8);

} // namespace ri::editor
