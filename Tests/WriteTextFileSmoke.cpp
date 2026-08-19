#include "RawIron/Core/Detail/JsonScan.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace fs = std::filesystem;

namespace {

std::string ReadAll(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "rawiron_write_text_file_smoke";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root, error);
    if (error) {
        return EXIT_FAILURE;
    }

    const fs::path target = root / "note.txt";
    if (!ri::core::detail::WriteTextFile(target, "first")
        || ReadAll(target) != "first"
        || !ri::core::detail::WriteTextFile(target, "second")
        || ReadAll(target) != "second") {
        fs::remove_all(root, error);
        return EXIT_FAILURE;
    }

    const fs::path directoryPath = root / "directory-as-file.txt";
    fs::create_directories(directoryPath, error);
    if (error || ri::core::detail::WriteTextFile(directoryPath, "nope")) {
        fs::remove_all(root, error);
        return EXIT_FAILURE;
    }

#if defined(_WIN32)
    {
        const fs::path victim = root / "victim.txt";
        const fs::path link = root / "link.txt";
        std::ofstream(victim, std::ios::binary) << "do-not-overwrite";
        fs::remove(link, error);
        const std::wstring linkW = link.wstring();
        const std::wstring victimW = victim.wstring();
        if (CreateSymbolicLinkW(linkW.c_str(), victimW.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
            || CreateSymbolicLinkW(linkW.c_str(), victimW.c_str(), 0)) {
            if (ri::core::detail::WriteTextFile(link, "overwrite-attempt")
                || ReadAll(victim) != "do-not-overwrite") {
                fs::remove_all(root, error);
                return EXIT_FAILURE;
            }
            fs::remove(link, error);
        }
    }
#endif

    fs::remove_all(root, error);
    return EXIT_SUCCESS;
}
