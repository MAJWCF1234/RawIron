#include "RawIron/Render/ProcessingStack.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void Write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary); file << text;
    Require(static_cast<bool>(file), "write test config");
}
int main(int argc, char** argv) {
    const auto scratch = fs::temp_directory_path() / ("rawiron-stacks-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{scratch};
    try {
        const auto active = scratch / "active.cfg";
        const auto serenity = scratch / "serenity/stack.cfg";
        const std::string defaults = "version=1\nbackend=serenity-color\n";
        Write(active, "stack=serenity\n"); Write(serenity, defaults);
        Write(scratch/"base/stack.cfg", "version=1\nbackend=base\n");
        ri::render::ProcessingStackReloader loader(active);
        std::string error;
        double now = 0;
        auto poll = [&] { now += 1; return loader.Poll(now, &error); };
        Require(poll() && error.empty(), "initial load");
        Require(loader.Current().serenity[0][3] == 0 && loader.Current().serenity[1][3] == .35f, "Bliss defaults");
        Require(loader.Current().serenity[7][0] == 1.f && loader.Current().serenity[6][3] == 31.f
                    && loader.Current().serenity[8][0] == 1.f && loader.Current().serenity[8][2] == 1.f
                    && loader.Current().serenity[8][3] == 1.f,
                "native stage defaults");
        Require(!poll() && loader.Revision() == 1, "unchanged does not publish");
        Require(loader.Current().exposureTuning[0] == 1.f
                    && loader.Current().purkinjeColor == std::array<float, 4>{.4f, .7f, 1.f, 5.f},
                "Bliss manual exposure and rod color defaults");
        Write(serenity, defaults + "SATURATION=0.4\nTONEMAP=ToneMap_Hejl2015\nBLOOM_STRENGTH=2\nExposure_Speed=2\nBLOOM_THRESHOLD=0.5\n");
        Require(poll() && loader.Current().serenity[0][1] == .4f && loader.Current().serenity[0][3] == 2
                    && loader.Current().serenity[7][0] == 2.f && loader.Current().serenity[8][0] == 2.f
                    && loader.Current().serenity[8][3] == .5f, "live controls");
        const auto lastGood = loader.Revision();
        for (const std::string bad : {"SATURATION=nan", "SATURATION=1e99", "SATURATION=2", "SATURATION=.1junk",
                "TONEMAP=bad", "DITHER=yes", "LPV_ENABLED=true", "SATURATION=0\nSATURATION=1", "partial edit",
                "Manual_exposure_value=0", "Manual_exposure_value=nan", "Purkinje_R=1.1", "Purkinje_Multiplier=10"}) {
            Write(serenity, defaults + bad + '\n');
            Require(!poll() && !error.empty() && loader.Revision() == lastGood && loader.Current().serenity[0][1] == .4f,
                    "invalid edits preserve last valid stack");
            Require(!poll() && error.empty(), "unchanged errors are not repeated");
        }
        Write(serenity, defaults);
        Require(poll() && loader.Current().serenity[0][1] == 0, "valid recovery resets removed controls");
        Write(serenity, defaults + "AUTO_EXPOSURE=false\nManual_exposure_value=0.5\nPurkinje_R=0.1\nPurkinje_Multiplier=2\n");
        Require(poll() && loader.Current().serenity[6][3] == 30.f
                    && loader.Current().exposureTuning[0] == .5f && loader.Current().purkinjeColor[0] == .1f
                    && loader.Current().purkinjeColor[3] == 2.f, "manual exposure and rod controls reload");
        Write(serenity, defaults);
        Require(poll() && loader.Current().exposureTuning[0] == 1.f && loader.Current().purkinjeColor[3] == 5.f,
                "removed exposure and rod controls restore shipped defaults");
        Write(active, "stack=base\n");
        Require(poll() && loader.Current().backend == ri::render::ProcessingStackBackend::Base, "live stack selection");
        loader.Select("serenity");
        Require(poll() && loader.Current().name == "serenity", "per-frame selection overrides file");
        loader.Select("");
        Require(poll() && loader.Current().name == "base", "release selection override");
        Write(active, "stack=../serenity\n");
        Require(!poll() && !error.empty() && loader.Current().name == "base", "path traversal rejected");
        Write(active, "stack=post\n");
        Write(scratch/"post/stack.cfg", "version=1\nbackend=post\nshader_cfg=shader.cfg\n");
        Write(scratch/"post/shader.cfg", "{\"replace\":true,\"post\":{\"bloom_intensity\":0.2}}");
        Require(poll() && loader.Current().post.loaded, "creator post stack");
        const auto postRevision = loader.Revision();
        Write(scratch/"post/shader.cfg", "{\"replace\":true,\"post\":{\"bloom_intensity\":0.4}}");
        Require(poll() && loader.Revision() > postRevision, "dependent shader.cfg live reload");
        Write(scratch/"post/stack.cfg", "version=1\nbackend=post\nshader_cfg=../../outside.cfg\n");
        Require(!poll() && !error.empty(), "dependency escapes rejected");
        const auto names = ri::render::DiscoverProcessingStacks(scratch);
        Require(names == std::vector<std::string>{"base","post","serenity"}, "discover creator stacks for cycling");
        if (argc > 1) {
            const fs::path library = fs::path(argv[1])/"Config/ProcessingStacks";
            for (const auto& name : ri::render::DiscoverProcessingStacks(library)) {
                ri::render::ProcessingStackReloader shipped(library/"active.cfg", name);
                Require(shipped.Poll(1,&error) && error.empty(), "shipped config loads");
            }
        }
        std::cout << "Processing stacks: defaults, switching, discovery, reload, atomic recovery and input validation passed\n";
        return 0;
    } catch (const std::exception& failure) { std::cerr << failure.what() << '\n'; return 1; }
}
