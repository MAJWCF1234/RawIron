#include "RawIron/GameHost/GameProjectBoot.h"
#include "RawIron/Core/CommandLine.h"

#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main(int argc, char** argv) {
    try {
        Require(argc==2,"workspace argument required");
        ri::gamehost::GameProjectBootOptions options{
            .workspaceRoot=argv[1], .gameId="cube-test",
            .expectedRuntimeModule="RawIron.Game.CubeTest",
            .configContract={.networkTuningMounted=true},
        };
        std::string error;
        auto project=ri::gamehost::PrepareGameProject(options,&error);
        Require(project.has_value(),error.c_str());
        Require(project->Manifest().id=="cube-test","resolved wrong project");
        Require(project->paths.gameRoot==project->Manifest().rootPath,"runtime game path differs from manifest");
        auto runtime=project->CreateRuntime();
        char program[]="boot-test"; char* commandArgs[]={program};
        ri::core::CommandLine commandLine(1,commandArgs);
        Require(ri::games::StartupGameRuntimeCore(runtime,commandLine,&error),error.c_str());
        Require(runtime.Context().Services().Resolve<ri::content::GameManifest>()==project->services.manifest,
            "manifest ownership changed during boot");
        Require(runtime.Context().Services().Resolve<ri::content::GameScriptBundle>()==project->services.scripts,
            "runtime must use the prepared script snapshot");
        Require(runtime.Frame(ri::games::BuildGameRuntimeFrameContext(0,1./60,0,0)),"runtime frame failed");
        runtime.Shutdown();
        Require(!runtime.Context().Services().Contains<ri::content::GameScriptBundle>(),
            "shutdown must restore the service registry");
        options.gameRoot=std::filesystem::path(argv[1])/"Games"/"LiminalHall";
        Require(!ri::gamehost::PrepareGameProject(options,&error)
            && error.find("but manifest requests")!=std::string::npos,
            "Cube Test executable must reject a different game module");
        options.expectedRuntimeModule="RawIron.Game.LiminalHall";
        options.checkpointStorageRoot=std::filesystem::path(argv[1])/"Saved"/"boot-test-save-override";
        auto liminal=ri::gamehost::PrepareGameProject(options,&error);
        Require(liminal.has_value(),error.c_str());
        Require(liminal->paths.saveRoot==options.checkpointStorageRoot,"checkpoint override was discarded");
        auto liminalRuntime=liminal->CreateRuntime();
        Require(ri::games::StartupGameRuntimeCore(liminalRuntime,commandLine,&error),error.c_str());
        Require(liminalRuntime.Context().Services().Resolve<ri::content::GameScriptBundle>()==liminal->services.scripts,
            "Liminal runtime reread or replaced prepared scripts");
        liminalRuntime.Shutdown();
        options.gameRoot=std::filesystem::path(argv[1])/"Games"/"missing-boot-fixture";
        Require(!ri::gamehost::PrepareGameProject(options,&error),"missing manifest accepted");
        std::cout << "Game project boot: resolution, ownership, lifecycle, module mismatch and missing manifest passed\n";
        return 0;
    } catch(const std::exception& error) {std::cerr << error.what() << '\n'; return 1;}
}
