#include "RawIron/Scene/HumanoidAnimationProfiles.h"

#include <cctype>
#include <unordered_map>

namespace ri::scene {
namespace {

std::string LowerCopy(std::string_view s) {
    std::string out(s.begin(), s.end());
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

using ProfileTable =
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>>;

const ProfileTable& ProfileClipTable() {
    static const ProfileTable k = [] {
        ProfileTable m;
        auto add = [&](std::string_view profile, std::string_view action, std::initializer_list<const char*> clips) {
            std::vector<std::string> v;
            v.reserve(clips.size());
            for (const char* c : clips) {
                v.emplace_back(c);
            }
            m[std::string(profile)][LowerCopy(action)] = std::move(v);
        };
        // hazmat_survivor
        add("hazmat_survivor", "idle", {"Armature|Idle_Loop", "Armature|Crouch_Idle_Loop"});
        add("hazmat_survivor", "alert", {"Armature|Idle_Alert_Loop", "Armature|Idle_Loop"});
        add("hazmat_survivor", "talk", {"Armature|Idle_Talking_Loop", "Armature|Idle_Loop"});
        add("hazmat_survivor", "radio", {"Armature|Radio_Loop", "Armature|Idle_Talking_Loop"});
        add("hazmat_survivor", "walk", {"Armature|Walk_Formal_Loop", "Armature|Walk_Loop"});
        add("hazmat_survivor", "run", {"Armature|Sprint_Loop"});
        add("hazmat_survivor", "interact", {"Armature|Interact"});
        add("hazmat_survivor", "use", {"Armature|Use_Terminal", "Armature|Interact"});
        add("hazmat_survivor", "hit", {"Armature|Hit_Chest", "Armature|Hit_Head"});
        add("hazmat_survivor", "death", {"Armature|Death01"});
        // police_survivor
        add("police_survivor", "idle", {"Armature|Idle_Loop"});
        add("police_survivor", "alert", {"Armature|Idle_Alert_Loop", "Armature|Idle_Loop"});
        add("police_survivor", "talk", {"Armature|Idle_Talking_Loop", "Armature|Idle_Loop"});
        add("police_survivor", "radio", {"Armature|Radio_Loop", "Armature|Idle_Talking_Loop"});
        add("police_survivor", "walk", {"Armature|Walk_Formal_Loop", "Armature|Walk_Loop"});
        add("police_survivor", "run", {"Armature|Sprint_Loop"});
        add("police_survivor", "interact", {"Armature|Interact"});
        add("police_survivor", "use", {"Armature|Use_Terminal", "Armature|Interact"});
        add("police_survivor", "hit", {"Armature|Hit_Head", "Armature|Hit_Chest"});
        add("police_survivor", "death", {"Armature|Death01"});
        // firefighter_survivor
        add("firefighter_survivor", "idle", {"Armature|Idle_Loop"});
        add("firefighter_survivor", "alert", {"Armature|Idle_Alert_Loop", "Armature|Idle_Loop"});
        add("firefighter_survivor", "talk", {"Armature|Idle_Talking_Loop", "Armature|Idle_Loop"});
        add("firefighter_survivor", "radio", {"Armature|Radio_Loop", "Armature|Idle_Talking_Loop"});
        add("firefighter_survivor", "walk", {"Armature|Walk_Loop", "Armature|Walk_Formal_Loop"});
        add("firefighter_survivor", "run", {"Armature|Sprint_Loop"});
        add("firefighter_survivor", "interact", {"Armature|Interact"});
        add("firefighter_survivor", "use", {"Armature|Use_Terminal", "Armature|Interact"});
        add("firefighter_survivor", "hit", {"Armature|Hit_Chest", "Armature|Hit_Head"});
        add("firefighter_survivor", "death", {"Armature|Death01"});
        // doctor_survivor
        add("doctor_survivor", "idle", {"Armature|Idle_Loop"});
        add("doctor_survivor", "alert", {"Armature|Idle_Alert_Loop", "Armature|Idle_Loop"});
        add("doctor_survivor", "talk", {"Armature|Idle_Talking_Loop", "Armature|Idle_Loop"});
        add("doctor_survivor", "briefing", {"Armature|Briefing_Loop", "Armature|Idle_Talking_Loop"});
        add("doctor_survivor", "walk", {"Armature|Walk_Loop", "Armature|Walk_Formal_Loop"});
        add("doctor_survivor", "run", {"Armature|Sprint_Loop"});
        add("doctor_survivor", "interact", {"Armature|Interact"});
        add("doctor_survivor", "use", {"Armature|Use_Terminal", "Armature|Interact"});
        add("doctor_survivor", "hit", {"Armature|Hit_Chest", "Armature|Hit_Head"});
        add("doctor_survivor", "death", {"Armature|Death01"});
        // civilian_survivor
        add("civilian_survivor", "idle", {"Armature|Idle_Loop"});
        add("civilian_survivor", "alert", {"Armature|Idle_Alert_Loop", "Armature|Idle_Loop"});
        add("civilian_survivor", "talk", {"Armature|Idle_Talking_Loop", "Armature|Idle_Loop"});
        add("civilian_survivor", "walk", {"Armature|Walk_Formal_Loop", "Armature|Walk_Loop"});
        add("civilian_survivor", "run", {"Armature|Sprint_Loop"});
        add("civilian_survivor", "interact", {"Armature|Interact"});
        add("civilian_survivor", "cower", {"Armature|Crouch_Idle_Loop", "Armature|Idle_Loop"});
        add("civilian_survivor", "hit", {"Armature|Hit_Chest", "Armature|Hit_Head"});
        add("civilian_survivor", "death", {"Armature|Death01"});
        return m;
    }();
    return k;
}

const std::vector<std::string>& EmptyClipList() {
    static const std::vector<std::string> k{};
    return k;
}

} // namespace

bool HumanoidAnimationProfileExists(std::string_view profileId) {
    const std::string key(profileId.begin(), profileId.end());
    return ProfileClipTable().contains(key);
}

const std::vector<std::string>& HumanoidProfileClipCandidates(std::string_view profileId, std::string_view action) {
    const ProfileTable& table = ProfileClipTable();
    const std::string pkey(profileId.begin(), profileId.end());
    const auto pit = table.find(pkey);
    if (pit == table.end()) {
        return EmptyClipList();
    }
    const std::string akey = LowerCopy(action);
    const auto ait = pit->second.find(akey);
    if (ait == pit->second.end()) {
        return EmptyClipList();
    }
    return ait->second;
}

std::vector<std::string_view> ListHumanoidAnimationProfileIds() {
    static constexpr std::string_view kIds[] = {
        "hazmat_survivor",
        "police_survivor",
        "firefighter_survivor",
        "doctor_survivor",
        "civilian_survivor",
    };
    return {std::begin(kIds), std::end(kIds)};
}

} // namespace ri::scene
