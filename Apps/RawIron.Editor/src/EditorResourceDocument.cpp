#include "EditorResourceDocument.h"

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Core/Detail/JsonScan.h"

#include <optional>
#include <system_error>

namespace ri::editor {

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kMaxResourceFileBytes = 512U * 1024U;

} // namespace

ResourceDocumentData LoadResourceDocument(const WorkspaceResourceEntry& entry) {
    ResourceDocumentData document{};
    document.absolutePath = entry.absolutePath;

    if (entry.category == WorkspaceResourceCategory::Manifest) {
        const std::optional<ri::content::GameManifest> manifest =
            ri::content::LoadGameManifest(document.absolutePath);
        if (manifest.has_value()) {
            document.manifestIssues = ri::content::ValidateGameProjectFormat(*manifest);
        } else {
            document.manifestIssues.push_back("Unable to parse manifest.json.");
        }
    }

    std::error_code ec{};
    const std::uintmax_t fileSize = fs::file_size(document.absolutePath, ec);
    if (ec || fileSize > static_cast<std::uintmax_t>(kMaxResourceFileBytes)) {
        document.auxMessage =
            ec ? std::string("Unable to read file metadata.") : std::string("File too large for embedded editor.");
        return document;
    }

    if (!IsLikelyTextResourcePath(document.absolutePath)) {
        document.auxMessage =
            "Binary / unknown extension - use Explorer to open beside the workspace.";
        return document;
    }

    document.utf8 = ri::core::detail::ReadTextFile(document.absolutePath);
    document.isTextEditable = true;
    return document;
}

bool SaveResourceDocumentUtf8(const fs::path& absolutePath, const std::string& utf8) {
    if (absolutePath.empty()) {
        return false;
    }
    return ri::core::detail::WriteTextFile(absolutePath, utf8);
}

} // namespace ri::editor
