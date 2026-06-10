#include "EditorProjectHealth.h"

#include "RawIron/Content/GameManifest.h"

#include <filesystem>

namespace ri::editor {

namespace fs = std::filesystem;

namespace {

void AddIfMissing(std::vector<std::string>& warnings,
                  const fs::path& gameRoot,
                  const std::string_view relativePath,
                  const std::string_view label) {
    std::error_code ec{};
    const fs::path path = gameRoot / relativePath;
    if (!fs::exists(path, ec)) {
        warnings.push_back(std::string(label) + " missing (" + std::string(relativePath) + ") — Export (Ctrl+E) or Create tab scaffold");
    }
}

} // namespace

ProjectHealthReport BuildProjectHealthReport(const ri::content::GameManifest& manifest,
                                             const bool dedicatedPlaytestExeExists,
                                             const bool genericPlaytestHostAvailable) {
    ProjectHealthReport report{};
    report.dedicatedPlaytestExeExists = dedicatedPlaytestExeExists;
    report.genericPlaytestHostAvailable = genericPlaytestHostAvailable;

    const fs::path gameRoot = manifest.rootPath;
    std::error_code ec{};
    if (!fs::exists(gameRoot / "manifest.json", ec)) {
        report.warnings.push_back("manifest.json missing in game root");
    }

    const std::string primaryLevel = manifest.primaryLevel.empty()
        ? std::string("levels/assembly.primitives.csv")
        : manifest.primaryLevel;
    AddIfMissing(report.warnings, gameRoot, primaryLevel, "Primary level");

    if (ri::content::DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(gameRoot, primaryLevel))
        != "missing") {
        report.readyNotes.push_back("Primary level present");
    }

    AddIfMissing(report.warnings, gameRoot, "assets/materials.manifest", "Materials manifest");
    AddIfMissing(report.warnings, gameRoot, "ui/main.ui.json", "UI layout");
    AddIfMissing(report.warnings, gameRoot, "levels/assembly.lighting.csv", "Lighting CSV");
    AddIfMissing(report.warnings, gameRoot, "levels/assembly.colliders.csv", "Colliders CSV");
    AddIfMissing(report.warnings, gameRoot, "levels/assembly.triggers.csv", "Triggers CSV");

    if (dedicatedPlaytestExeExists) {
        report.readyNotes.push_back("Dedicated playtest executable found");
    } else if (genericPlaytestHostAvailable) {
        report.readyNotes.push_back("Playtest uses sandbox host with --game-root");
    } else {
        report.warnings.push_back("No playtest executable — build RawIron.MultiplayerSandboxGame or your game App target");
    }

    if (report.warnings.empty()) {
        report.readyNotes.push_back("Project scaffold looks complete");
    }

    return report;
}

std::string SummarizeProjectHealthForWelcome(const ProjectHealthReport& report) {
    std::string summary;
    if (!report.readyNotes.empty()) {
        summary += "Ready: " + report.readyNotes.front();
        for (std::size_t index = 1; index < report.readyNotes.size() && index < 2U; ++index) {
            summary += " · " + report.readyNotes[index];
        }
    }
    if (!report.warnings.empty()) {
        if (!summary.empty()) {
            summary += "\n";
        }
        summary += "Needs attention: " + report.warnings.front();
        for (std::size_t index = 1; index < report.warnings.size() && index < 3U; ++index) {
            summary += " · " + report.warnings[index];
        }
    }
    if (summary.empty()) {
        summary = "Select a scene node to edit transforms, materials, and lights.";
    }
    return summary;
}

FilesPanelHealthSummary SummarizeProjectHealthForFilesPanel(const ProjectHealthReport& report) {
    FilesPanelHealthSummary summary{};
    if (!report.readyNotes.empty()) {
        summary.readyLine = "Ready: " + report.readyNotes.front();
        for (std::size_t index = 1; index < report.readyNotes.size() && index < 3U; ++index) {
            summary.readyLine += " · " + report.readyNotes[index];
        }
    } else if (report.warnings.empty()) {
        summary.readyLine = "Ready: project scaffold looks complete";
    } else {
        summary.readyLine = "Needs attention before playtest";
    }
    summary.warnings = report.warnings;
    return summary;
}

} // namespace ri::editor
