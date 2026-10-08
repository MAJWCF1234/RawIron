#include "RawIron/Games/LiminalHall/LiminalHallWorld.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition,const std::string& message) {
    if(!condition) throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
}
int main(int argc,char** argv) {
    namespace fs=std::filesystem;
    fs::path fixture;
    try {
        Require(argc==2,"workspace argument required");
        const fs::path workspace=argv[1];
        const auto game=workspace/"Games"/"LiminalHall";
        fixture=fs::temp_directory_path()/("liminal-boot-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(fixture);
        for(const char* folder:{"ai","assets","config","data","levels","plugins","scripts","tests","ui"})
            fs::copy(game/folder,fixture/folder,fs::copy_options::recursive);
        for(const char* file:{"manifest.json","README.md"})fs::copy_file(game/file,fixture/file);
        ri::games::liminal::StandaloneOptions launch{};
        launch.workspaceRoot=workspace;launch.gameRoot=workspace/"Games"/"CubeTest";
        launch.captureMouse=false;
        ri::games::liminal::HeadlessCaptureOptions capture{};
        capture.standalone=launch;capture.frames=1;capture.autoplay=false;
        std::string error;
        Require(!ri::games::liminal::RunStandalone(launch,&error)
            && error.find("but manifest requests")!=std::string::npos,"desktop accepted another game's runtime module");
        Require(!ri::games::liminal::RunHeadlessCapture(capture,&error)
            && error.find("but manifest requests")!=std::string::npos,"headless accepted another game's runtime module");
        const auto physics=fixture/"scripts"/"physics.riscript";
        const auto authored=Read(physics);
        std::ofstream(physics,std::ios::app)<<"\ngravity_scale=broken\n";
        launch.gameRoot=fixture;capture.standalone=launch;
        const bool invalidDesktop=ri::games::liminal::RunStandalone(launch,&error);
        Require(!invalidDesktop && error.find("physics.riscript")!=std::string::npos,
            "desktop did not reject malformed tuning in shared preflight: "+error);
        const bool invalidHeadless=ri::games::liminal::RunHeadlessCapture(capture,&error);
        Require(!invalidHeadless && error.find("physics.riscript")!=std::string::npos,
            "headless did not reject malformed tuning in shared preflight: "+error);
        std::ofstream(physics)<<authored;
        capture.standalone.gameRoot=game;
        capture.standalone.width=128;capture.standalone.height=72;
        capture.standalone.checkpointStorageRoot=fixture/"saves";
        capture.outputPath=fixture/"capture.bmp";
        Require(ri::games::liminal::RunHeadlessCapture(capture,&error),"valid Liminal headless boot failed: "+error);
        Require(fs::file_size(capture.outputPath)>100,"headless boot produced no image");
        fs::remove_all(fixture);
        std::cout<<"Liminal shared host: desktop/headless preflight, module identity, malformed scripts and actual headless frame passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::error_code cleanup;
        if(!fixture.empty())fs::remove_all(fixture,cleanup);
        std::cerr<<error.what()<<'\n';return 1;
    }
}
