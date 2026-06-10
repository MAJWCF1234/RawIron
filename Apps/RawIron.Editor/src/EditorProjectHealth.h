#pragma once

#include "RawIron/Content/GameManifest.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ri::editor {

struct ProjectHealthReport {
    std::vector<std::string> warnings;
    std::vector<std::string> readyNotes;
    bool dedicatedPlaytestExeExists = false;
    bool genericPlaytestHostAvailable = false;
};

struct FilesPanelHealthSummary {
    std::string readyLine;
    std::vector<std::string> warnings;
};

[[nodiscard]] ProjectHealthReport BuildProjectHealthReport(
    const ri::content::GameManifest& manifest,
    bool dedicatedPlaytestExeExists,
    bool genericPlaytestHostAvailable);

[[nodiscard]] std::string SummarizeProjectHealthForWelcome(const ProjectHealthReport& report);

[[nodiscard]] FilesPanelHealthSummary SummarizeProjectHealthForFilesPanel(const ProjectHealthReport& report);

} // namespace ri::editor
