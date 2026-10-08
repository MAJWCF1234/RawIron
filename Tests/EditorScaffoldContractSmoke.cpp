#include "EditorProjectScaffolding.h"
#include "RawIron/Games/GameConfigContracts.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    const auto root = std::filesystem::temp_directory_path()/
        ("rawiron-scaffold-contract-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ri::content::GameManifest manifest{};
    manifest.id="contract-fixture";
    manifest.name="Contract fixture";
    manifest.rootPath=root;
    std::size_t count=0;
    std::vector<std::string> files;
    std::string error;
    bool ok = ri::editor::EnsureMountedGameScaffold(manifest,count,files,&error)
        && ri::games::EnforceGameConfigContracts(root,
            {.mode=ri::games::GameConfigContractMode::Strict}, &error);
    // Scaffolding an existing project must retain its author-owned tuning.
    if (ok) {
        std::ofstream(root/"scripts"/"physics.riscript", std::ios::app) << "# author-owned marker\n";
        ok = ri::editor::EnsureMountedGameScaffold(manifest,count,files,&error) && count==0;
        std::ifstream input(root/"scripts"/"physics.riscript");
        const std::string text{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
        ok = ok && text.find("author-owned marker")!=std::string::npos;
    }
    std::filesystem::remove_all(root);
    if (!ok) { std::cerr << "Scaffold contract failed: " << error << '\n'; return 1; }
    std::cout << "Editor scaffold: strict tuning contract and author-owned files preserved\n";
    return 0;
}
