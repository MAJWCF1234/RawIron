#include "RawIron/Content/NativeAnimationDocument.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

int main() {
    namespace fs = std::filesystem;
    ri::content::NativeAnimationDocument clip =
        ri::content::CreateNativeAnimationDocument("wave", "Wave", "rigs/humanoid.ri_rig.json");
    clip.tracks.push_back(ri::content::NativeAnimationTrack{
        .boneName = "hips",
        .keys = {
            {.timeSeconds = 0.0, .rotationDegrees = {}, .scale = {1.0F, 1.0F, 1.0F}},
            {.timeSeconds = 0.5, .rotationDegrees = {0.0F, 25.0F, 0.0F}, .scale = {1.0F, 1.0F, 1.0F}},
        },
    });
    clip.events.push_back(ri::content::NativeAnimationEvent{.timeSeconds = 0.5, .name = "foot_l"});
    const auto report = ri::content::ValidateNativeAnimationDocument(clip);
    if (!report.valid || report.trackCount != 1U || report.keyCount != 2U || report.eventCount != 1U) {
        std::cerr << "Seeded animation failed validation.\n";
        return EXIT_FAILURE;
    }
    const std::string json = ri::content::SerializeNativeAnimationDocument(clip);
    const auto parsed = ri::content::ParseNativeAnimationDocument(json);
    if (!parsed.has_value() || parsed->tracks.size() != 1U || parsed->tracks.front().keys.size() != 2U
        || parsed->tracks.front().keys.back().rotationDegrees.y != 25.0F
        || parsed->rigPath != "rigs/humanoid.ri_rig.json"
        || !parsed->rootMotion
        || parsed->events.size() != 1U || parsed->events.front().name != "foot_l"
        || parsed->events.front().timeSeconds != 0.5) {
        std::cerr << "Animation document did not round trip.\n";
        return EXIT_FAILURE;
    }
    const auto legacy = ri::content::ParseNativeAnimationDocument(
        "{\n"
        "  \"formatVersion\": 1,\n"
        "  \"id\": \"legacy\",\n"
        "  \"displayName\": \"Legacy\",\n"
        "  \"rigPath\": \"rigs/humanoid.ri_rig.json\",\n"
        "  \"durationSeconds\": 1,\n"
        "  \"looping\": true,\n"
        "  \"tracks\": [{\"boneName\":\"hips\",\"keys\":[{\"timeSeconds\":0,"
        "\"translation\":{\"x\":0,\"y\":0,\"z\":0},\"rotationDegrees\":{\"x\":0,\"y\":0,\"z\":0},"
        "\"scale\":{\"x\":1,\"y\":1,\"z\":1}}]}]\n"
        "}\n");
    if (!legacy.has_value() || !legacy->events.empty() || !legacy->rootMotion
        || ri::content::ValidateNativeAnimationDocument(*legacy).eventCount != 0U) {
        std::cerr << "Legacy clip without events/rootMotion did not load.\n";
        return EXIT_FAILURE;
    }
    const fs::path temp = fs::temp_directory_path() / "rawiron_anim_event_smoke.ri_anim.json";
    if (!ri::content::SaveNativeAnimationDocument(temp, clip)) {
        std::cerr << "Could not save clip events to disk.\n";
        fs::remove(temp);
        return EXIT_FAILURE;
    }
    const auto loaded = ri::content::LoadNativeAnimationDocument(temp);
    fs::remove(temp);
    if (!loaded.has_value() || loaded->events.size() != 1U || loaded->events.front().name != "foot_l") {
        std::cerr << "Disk load dropped clip events.\n";
        return EXIT_FAILURE;
    }
    clip.rootMotion = false;
    const auto inPlace = ri::content::ParseNativeAnimationDocument(
        ri::content::SerializeNativeAnimationDocument(clip));
    if (!inPlace.has_value() || inPlace->rootMotion) {
        std::cerr << "rootMotion false did not round trip.\n";
        return EXIT_FAILURE;
    }
    clip.events.front().name.clear();
    if (ri::content::ValidateNativeAnimationDocument(clip).valid) {
        std::cerr << "Nameless animation event was accepted.\n";
        return EXIT_FAILURE;
    }
    clip.events.clear();
    clip.tracks.front().keys.front().scale.x = 0.0F;
    if (ri::content::ValidateNativeAnimationDocument(clip).valid) {
        std::cerr << "Zero scale key was accepted.\n";
        return EXIT_FAILURE;
    }
    std::cout << "Native animation document smoke passed.\n";
    return EXIT_SUCCESS;
}
