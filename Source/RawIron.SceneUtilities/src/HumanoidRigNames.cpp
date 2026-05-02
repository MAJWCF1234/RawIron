#include "RawIron/Scene/HumanoidRigNames.h"

#include <cctype>
#include <string>
#include <unordered_map>

namespace ri::scene {
namespace {

void ToLowerInPlace(std::string& s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
}

bool StartsWith(const std::string& s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

void StripMixamoPrefix(std::string& s) {
    if (s.size() >= 9U && s.compare(0, 9, "mixamorig") == 0) {
        s.erase(0, 9);
        if (!s.empty() && (s.front() == ':' || s.front() == '_')) {
            s.erase(0, 1);
        }
    }
}

void StripArmaturePrefix(std::string& s) {
    if (StartsWith(s, "armature|")) {
        s.erase(0, 9);
    }
}

void KeepAlnumOnly(std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
            out.push_back(c);
        }
    }
    s = std::move(out);
}

const std::unordered_map<std::string, std::string>& CompactAliases() {
    static const std::unordered_map<std::string, std::string> k = {
        {"hip", "hips"},
        {"hips", "hips"},
        {"pelvis", "hips"},
        {"spine01", "spine"},
        {"spine1", "spine"},
        {"spine02", "chest"},
        {"spine2", "chest"},
        {"spine03", "chest"},
        {"spine3", "chest"},
        {"upperchest", "chest"},
        {"neck01", "neck"},
        {"claviclel", "leftshoulder"},
        {"clavicler", "rightshoulder"},
        {"upperarml", "leftarm"},
        {"upperarmr", "rightarm"},
        {"lowerarml", "leftforearm"},
        {"lowerarmr", "rightforearm"},
        {"handl", "lefthand"},
        {"handr", "righthand"},
        {"thighl", "leftupleg"},
        {"upperlegl", "leftupleg"},
        {"thighr", "rightupleg"},
        {"upperlegr", "rightupleg"},
        {"calfl", "leftleg"},
        {"lowerlegl", "leftleg"},
        {"calfr", "rightleg"},
        {"lowerlegr", "rightleg"},
        {"footl", "leftfoot"},
        {"footr", "rightfoot"},
        {"balll", "lefttoebase"},
        {"ballr", "righttoebase"},
    };
    return k;
}

} // namespace

std::string CanonicalHumanoidBoneKey(const std::string_view rawBoneName) {
    if (rawBoneName.empty()) {
        return {};
    }
    std::string s(rawBoneName);
    ToLowerInPlace(s);
    StripMixamoPrefix(s);
    StripArmaturePrefix(s);
    KeepAlnumOnly(s);
    if (s.empty()) {
        return {};
    }
    const auto& aliases = CompactAliases();
    if (const auto it = aliases.find(s); it != aliases.end()) {
        return it->second;
    }
    return s;
}

} // namespace ri::scene
