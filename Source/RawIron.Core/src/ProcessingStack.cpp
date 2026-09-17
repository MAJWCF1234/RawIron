#include "RawIron/Render/ProcessingStack.h"

#include <charconv>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string_view>

namespace ri::render {
namespace {
std::string Trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == text.npos) return {};
    return std::string(text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1));
}
std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read processing stack file: " + path.string());
    std::string text(65537, '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(input.gcount()));
    if (text.size() > 65536) throw std::runtime_error("Processing stack file exceeds 64 KiB");
    if (text.starts_with("\xef\xbb\xbf")) text.erase(0, 3);
    return text;
}
using Values = std::map<std::string, std::string>;
Values Parse(std::string_view text) {
    Values values;
    while (!text.empty()) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        text = end == text.npos ? std::string_view{} : text.substr(end + 1);
        line = line.substr(0, line.find('#'));
        const std::string trimmed = Trim(line);
        if (trimmed.empty()) continue;
        const auto equals = trimmed.find('=');
        if (equals == trimmed.npos) throw std::runtime_error("Expected key=value in processing stack");
        const auto key = Trim(std::string_view(trimmed).substr(0, equals));
        const auto value = Trim(std::string_view(trimmed).substr(equals + 1));
        if (key.empty() || value.empty() || !values.emplace(key, value).second)
            throw std::runtime_error("Empty or duplicate processing stack key: " + key);
    }
    return values;
}
std::string Take(Values& values, const std::string& key, const std::string& fallback = {}) {
    const auto found = values.find(key);
    if (found == values.end()) return fallback;
    auto value = found->second;
    values.erase(found);
    return value;
}
float Number(Values& values, const std::string& key, float fallback, float low, float high) {
    const auto text = Take(values, key);
    if (text.empty()) return fallback;
    float value = 0;
    const auto result = std::from_chars(text.data(), text.data()+text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data()+text.size()
        || !std::isfinite(value) || value < low || value > high)
        throw std::runtime_error("Invalid or out-of-range processing stack setting: " + key + "=" + text);
    return value;
}
float Flag(Values& values, const std::string& key, float fallback) {
    const auto text = Take(values, key);
    if (text.empty()) return fallback;
    if (text == "true") return 1;
    if (text == "false") return 0;
    throw std::runtime_error("Expected true or false for " + key);
}
void Exhausted(const Values& values) {
    if (!values.empty()) throw std::runtime_error("Unknown or unported processing stack setting: " + values.begin()->first);
}
bool ValidName(std::string_view name) {
    if (name.empty() || name.size() > 64) return false;
    for (char c : name) if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '-' && c != '_') return false;
    return true;
}
std::filesystem::path Child(const std::filesystem::path& root, const std::filesystem::path& relative) {
    const auto canonicalRoot = std::filesystem::weakly_canonical(root);
    const auto path = std::filesystem::weakly_canonical(root / relative);
    const auto local = path.lexically_relative(canonicalRoot);
    if (local.empty() || local.is_absolute() || *local.begin() == "..")
        throw std::runtime_error("Processing stack dependency escapes its library");
    return path;
}
void LoadSerenity(Values& values, ProcessingStack& stack) {
    auto& p = stack.serenity;
    p[0][0] = Number(values, "EXPOSURE_MULTIPLIER", 1, .0001f, 32);
    p[0][1] = Number(values, "SATURATION", 0, -1, 1);
    p[0][2] = Number(values, "CROSSTALK", 0, -1, 1);
    constexpr std::array names{"ToneMap_AgX_minimal", "ToneMap_AgX", "ToneMap_Hejl2015", "Tonemap_Xonk",
        "Tonemap_Uchimura", "HableTonemap", "Full_Reinhard_Edit", "Tonemap_Full_Reinhard", "reinhard", "Tonemap_Lottes", "ACESFilm"};
    const auto tone = Take(values, "TONEMAP", names[0]);
    bool found = false;
    for (std::size_t i = 0; i < names.size(); ++i) if (tone == names[i]) { p[0][3] = static_cast<float>(i); found = true; }
    if (!found) throw std::runtime_error("Unknown Serenity TONEMAP: " + tone);
    p[1][0] = Number(values, "UPPER_CURVE", 0, -2, 2);
    p[1][1] = Number(values, "LOWER_CURVE", 0, -2, 2);
    p[1][2] = Flag(values, "DITHER", 1);
    p[1][3] = Number(values, "SHARPENING", .35f, 0, 2);
    constexpr std::array prefixes{"SHADOWS_GRADE_", "MIDS_GRADE_", "HIGHLIGHTS_GRADE_"};
    constexpr std::array suffixes{"R", "G", "B", "MUL"};
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 4; ++j)
        p[i+2][j] = Number(values, std::string(prefixes[i])+suffixes[j], 1, 0, 2);
    p[5][0] = Flag(values, "COLOR_GRADING_ENABLED", 0);
    p[5][1] = Flag(values, "LUMINANCE_CURVE", 0);
    p[5][2] = Flag(values, "USE_ACES_COLORSPACE_APPROXIMATION", 0);
    p[5][3] = Flag(values, "CONTRAST_ADAPTATIVE_SHARPENING", 1);
    const float autoExposure = Flag(values, "AUTO_EXPOSURE", 1);
    const float taa = Flag(values, "TAA", 1);
    const float clouds = Flag(values, "VOLUMETRIC_CLOUDS", 1);
    const float purkinje = Flag(values, "Fake_purkinje", 1);
    const float vlFog = Flag(values, "TOGGLE_VL_FOG", 1);
    p[6][3] = autoExposure + taa * 2 + clouds * 4 + purkinje * 8 + vlFog * 16;
    p[7][0] = Number(values, "BLOOM_STRENGTH", 1, 0, 100);
    p[7][1] = Number(values, "BLOOMY_FOG", 1.5f, 0, 20);
    p[7][2] = Number(values, "Purkinje_strength", 1, 0, 1);
    p[7][3] = Number(values, "BLEND_FACTOR", .12f, .01f, 1);
    p[8][0] = Number(values, "Exposure_Speed", 1, .25f, 5);
    p[8][1] = Number(values, "CLOUD_SHADOW_STRENGTH", 1, 0, 1);
    p[8][2] = Number(values, "WATER_REFRACTION", 1, 0, 2);
    p[8][3] = Number(values, "BLOOM_THRESHOLD", 1, 0, 8);
    stack.exposureTuning[0] = Number(values, "Manual_exposure_value", 1, .000553f, 12.18249f);
    stack.purkinjeColor[0] = Number(values, "Purkinje_R", .4f, .01f, 1);
    stack.purkinjeColor[1] = Number(values, "Purkinje_G", .7f, .01f, 1);
    stack.purkinjeColor[2] = Number(values, "Purkinje_B", 1, .01f, 1);
    stack.purkinjeColor[3] = Number(values, "Purkinje_Multiplier", 5, .05f, 9.95f);
}
}

ProcessingStackReloader::ProcessingStackReloader(std::filesystem::path selectionFile, std::string overrideName)
    : selectionFile_(std::move(selectionFile)), overrideName_(std::move(overrideName)) {}

std::vector<std::string> DiscoverProcessingStacks(const std::filesystem::path& root) {
    std::vector<std::string> result;
    std::error_code error;
    for (std::filesystem::directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        const auto name = it->path().filename().string();
        if (ValidName(name) && std::filesystem::is_regular_file(it->path()/"stack.cfg", error)) result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}

void ProcessingStackReloader::Select(std::string name) {
    if (overrideName_ == name) return;
    overrideName_ = std::move(name);
    nextPoll_ = 0;
}

bool ProcessingStackReloader::Poll(double now, std::string* error) {
    if (error) error->clear();
    if (selectionFile_.empty() || !std::isfinite(now) || now < nextPoll_) return false;
    nextPoll_ = now + .25;
    try {
        auto selectionText = Read(selectionFile_);
        auto selection = Parse(selectionText);
        const auto configuredName = Take(selection, "stack");
        Exhausted(selection);
        const auto name = overrideName_.empty() ? configuredName : overrideName_;
        if (!ValidName(name)) throw std::runtime_error("Invalid processing stack name");
        const auto root = selectionFile_.parent_path();
        const auto profile = Child(root, std::filesystem::path(name)/"stack.cfg");
        const auto profileText = Read(profile);
        auto values = Parse(profileText);
        if (Take(values, "version") != "1") throw std::runtime_error("Unsupported processing stack version");
        ProcessingStack candidate;
        candidate.name = name;
        const auto backend = Take(values, "backend");
        std::string signature = name + '\0' + selectionText + '\0' + profileText;
        if (backend == "serenity-color") {
            candidate.backend = ProcessingStackBackend::SerenityColor;
            LoadSerenity(values, candidate);
        } else if (backend == "post") {
            candidate.backend = ProcessingStackBackend::Post;
            const auto dependency = Child(profile.parent_path(), Take(values, "shader_cfg"));
            const auto dependencyText = Read(dependency);
            signature += '\0' + dependencyText;
            Exhausted(values);
            if (signature == previousContent_) { previousError_.clear(); return false; }
            std::string reason;
            if (!LoadShaderCfg(dependency, &candidate.post, &reason)) throw std::runtime_error(reason);
            if (Read(dependency) != dependencyText) throw std::runtime_error("Processing stack changed during reload; retrying");
        } else if (backend != "base") throw std::runtime_error("Unknown processing stack backend: " + backend);
        Exhausted(values);
        if (signature == previousContent_) { previousError_.clear(); return false; }
        current_ = std::move(candidate);
        previousContent_ = std::move(signature);
        previousError_.clear();
        ++revision_;
        return true;
    } catch (const std::exception& failure) {
        if (previousError_ != failure.what() && error) *error = failure.what();
        previousError_ = failure.what();
        return false;
    }
}
} // namespace ri::render
