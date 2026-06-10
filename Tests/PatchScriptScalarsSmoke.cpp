#include "RawIron/Content/ScriptScalars.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    const fs::path tempPath = fs::temp_directory_path() / "rawiron_patch_script_scalars.riscript";
    {
        std::ofstream writer(tempPath, std::ios::trunc);
        writer << "# sample\n"
               << "clear_top_r=0.84\n"
               << "fog_far_r=0.84\n"
               << "native_exposure=1.0\n";
    }

    const ri::content::ScriptScalarMap patches = ri::content::LoadScriptScalarsFromText(
        "clear_top_r=0.55\nfog_r=0.48\n");
    if (!ri::content::PatchScriptScalarsFile(tempPath, patches)) {
        return EXIT_FAILURE;
    }

    std::ifstream reader(tempPath);
    const std::string patchedText{
        std::istreambuf_iterator<char>(reader),
        std::istreambuf_iterator<char>(),
    };
    if (patchedText.find("clear_top_r=0.55") == std::string::npos) {
        return EXIT_FAILURE;
    }
    if (patchedText.find("fog_far_r=0.84") == std::string::npos) {
        return EXIT_FAILURE;
    }
    if (patchedText.find("native_exposure=1.0") == std::string::npos) {
        return EXIT_FAILURE;
    }
    if (patchedText.find("fog_r=0.48") == std::string::npos) {
        return EXIT_FAILURE;
    }

    std::error_code ec{};
    fs::remove(tempPath, ec);
    return EXIT_SUCCESS;
}
