#include "RawIron/World/CheckpointPersistence.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace ri::world {
namespace {

std::string Trim(std::string value) {
    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char ch) { return !isSpace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char ch) { return !isSpace(ch); }).base(), value.end());
    return value;
}

bool ParseDouble(std::string_view text, double& value) {
    std::string parsedText = Trim(std::string(text));
    if (parsedText.empty()) {
        return false;
    }
    char* end = nullptr;
    value = std::strtod(parsedText.c_str(), &end);
    return end != nullptr && *end == '\0' && std::isfinite(value);
}

bool ParseVec3(std::string_view text, ri::math::Vec3& out) {
    std::string copy(text);
    std::stringstream stream(copy);
    std::string component;
    std::vector<double> values;
    while (std::getline(stream, component, ',')) {
        double parsed = 0.0;
        if (!ParseDouble(Trim(component), parsed)) {
            return false;
        }
        values.push_back(parsed);
    }
    if (values.size() != 3U) {
        return false;
    }
    out = ri::math::Vec3{
        static_cast<float>(values[0]),
        static_cast<float>(values[1]),
        static_cast<float>(values[2]),
    };
    return true;
}

std::string ToVec3String(const ri::math::Vec3& value) {
    std::ostringstream stream;
    stream << value.x << "," << value.y << "," << value.z;
    return stream.str();
}

std::string UrlDecode(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (c == '+') {
            decoded.push_back(' ');
            continue;
        }
        if (c == '%' && (i + 2U) < value.size()) {
            const char hi = value[i + 1U];
            const char lo = value[i + 2U];
            auto hexToInt = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') {
                    return ch - '0';
                }
                if (ch >= 'a' && ch <= 'f') {
                    return 10 + (ch - 'a');
                }
                if (ch >= 'A' && ch <= 'F') {
                    return 10 + (ch - 'A');
                }
                return -1;
            };
            const int hiValue = hexToInt(hi);
            const int loValue = hexToInt(lo);
            if (hiValue >= 0 && loValue >= 0) {
                decoded.push_back(static_cast<char>((hiValue << 4) | loValue));
                i += 2U;
                continue;
            }
        }
        decoded.push_back(c);
    }
    return decoded;
}

std::vector<std::string> SplitCsv(std::string_view input) {
    std::vector<std::string> values;
    std::string copy(input);
    std::stringstream stream(copy);
    std::string token;
    while (std::getline(stream, token, ',')) {
        token = Trim(token);
        if (!token.empty()) {
            values.push_back(token);
        }
    }
    return values;
}

std::string JoinCsv(const std::vector<std::string>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0U) {
            stream << ",";
        }
        stream << values[index];
    }
    return stream.str();
}

bool ParseBool(std::string_view value) {
    const std::string lowered = [&]() {
        std::string v(value);
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return v;
    }();
    return lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
}

} // namespace

FileCheckpointStore::FileCheckpointStore(std::filesystem::path rootDirectory)
    : rootDirectory_(std::move(rootDirectory)) {}

bool FileCheckpointStore::Save(const RuntimeCheckpointSnapshot& snapshot, std::string* error) const {
    const ri::validation::RuntimeCheckpointState parsedState = ri::validation::ParseStoredCheckpointState(snapshot.state);
    if (const std::optional<std::string> validationError = ri::validation::ValidateCheckpointState(parsedState, "checkpoint");
        validationError.has_value()) {
        if (error != nullptr) {
            *error = *validationError;
        }
        return false;
    }

    std::error_code createError;
    std::filesystem::create_directories(rootDirectory_, createError);
    if (createError) {
        if (error != nullptr) {
            *error = "Failed to create checkpoint directory: " + createError.message();
        }
        return false;
    }

    std::ofstream output(SlotPath(snapshot.slot), std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        if (error != nullptr) {
            *error = "Failed to open checkpoint file for write.";
        }
        return false;
    }

    output << "version=1\n";
    output << "slot=" << snapshot.slot << "\n";
    output << "level=" << parsedState.level.value_or("") << "\n";
    output << "checkpointId=" << parsedState.checkpointId.value_or("") << "\n";
    output << "flags=" << JoinCsv(parsedState.flags) << "\n";
    output << "eventIds=" << JoinCsv(parsedState.eventIds) << "\n";
    for (const auto& [key, number] : parsedState.values) {
        output << "value:" << key << "=" << number << "\n";
    }
    if (snapshot.playerPosition.has_value()) {
        output << "playerPosition=" << ToVec3String(*snapshot.playerPosition) << "\n";
    }
    if (snapshot.playerRotation.has_value()) {
        output << "playerRotation=" << ToVec3String(*snapshot.playerRotation) << "\n";
    }

    if (!output.good()) {
        if (error != nullptr) {
            *error = "Failed to write checkpoint file.";
        }
        return false;
    }
    return true;
}

std::optional<RuntimeCheckpointSnapshot> FileCheckpointStore::Load(std::string_view slot, std::string* error) const {
    std::ifstream input(SlotPath(slot), std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }

    RuntimeCheckpointSnapshot snapshot{};
    snapshot.slot = std::string(slot);
    std::string line;
    while (std::getline(input, line)) {
        const std::size_t equalsIndex = line.find('=');
        if (equalsIndex == std::string::npos) {
            continue;
        }
        const std::string key = Trim(line.substr(0, equalsIndex));
        const std::string value = Trim(line.substr(equalsIndex + 1U));
        if (key == "level") {
            snapshot.state.level = value;
            continue;
        }
        if (key == "checkpointId") {
            if (!value.empty()) {
                snapshot.state.checkpointId = value;
            }
            continue;
        }
        if (key == "flags") {
            snapshot.state.flags = SplitCsv(value);
            continue;
        }
        if (key == "eventIds") {
            snapshot.state.eventIds = SplitCsv(value);
            continue;
        }
        if (key == "playerPosition") {
            ri::math::Vec3 parsed{};
            if (ParseVec3(value, parsed)) {
                snapshot.playerPosition = parsed;
            }
            continue;
        }
        if (key == "playerRotation") {
            ri::math::Vec3 parsed{};
            if (ParseVec3(value, parsed)) {
                snapshot.playerRotation = parsed;
            }
            continue;
        }
        if (key.rfind("value:", 0) == 0) {
            const std::string valueKey = key.substr(std::string("value:").size());
            double number = 0.0;
            if (!valueKey.empty() && ParseDouble(value, number) && std::isfinite(number)) {
                snapshot.state.values[valueKey] = number;
            }
        }
    }

    snapshot.state = ri::validation::ParseStoredCheckpointState(snapshot.state);
    if (const std::optional<std::string> validationError = ri::validation::ValidateCheckpointState(snapshot.state, "checkpoint");
        validationError.has_value()) {
        if (error != nullptr) {
            *error = *validationError;
        }
        return std::nullopt;
    }
    return snapshot;
}

bool FileCheckpointStore::Clear(std::string_view slot, std::string* error) const {
    std::error_code removalError;
    std::filesystem::remove(SlotPath(slot), removalError);
    if (removalError) {
        if (error != nullptr) {
            *error = "Failed to clear checkpoint file: " + removalError.message();
        }
        return false;
    }
    return true;
}

std::filesystem::path FileCheckpointStore::SlotPath(std::string_view slot) const {
    std::string safeSlot(slot);
    if (safeSlot.empty()) {
        safeSlot = "autosave";
    }
    for (char& ch : safeSlot) {
        const bool valid = std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '_';
        if (!valid) {
            ch = '_';
        }
    }
    return rootDirectory_ / (safeSlot + ".checkpoint");
}

CheckpointStartupOptions ParseCheckpointStartupOptions(std::string_view queryString) {
    CheckpointStartupOptions options{};
    std::string raw(queryString);
    if (!raw.empty() && raw.front() == '?') {
        raw.erase(raw.begin());
    }
    std::stringstream stream(raw);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        const std::size_t equalsIndex = pair.find('=');
        const std::string key = UrlDecode(Trim(pair.substr(0, equalsIndex)));
        const std::string value = equalsIndex == std::string::npos ? "" : UrlDecode(Trim(pair.substr(equalsIndex + 1U)));
        if (key == "startFromCheckpoint") {
            options.startFromCheckpoint = ParseBool(value);
        } else if (key == "checkpointSlot" && !value.empty()) {
            options.slot = value;
        }
    }
    return options;
}

CheckpointStartupDecision ResolveCheckpointStartupDecision(const CheckpointStartupOptions& options,
                                                           const FileCheckpointStore& store,
                                                           std::string* error) {
    CheckpointStartupOptions merged = options;
    if (merged.queryString.has_value()) {
        const CheckpointStartupOptions parsedFromQuery = ParseCheckpointStartupOptions(*merged.queryString);
        merged.startFromCheckpoint = merged.startFromCheckpoint || parsedFromQuery.startFromCheckpoint;
        if (!parsedFromQuery.slot.empty()) {
            merged.slot = parsedFromQuery.slot;
        }
    }

    CheckpointStartupDecision decision{};
    decision.startFromCheckpoint = merged.startFromCheckpoint;
    decision.slot = merged.slot.empty() ? std::string("autosave") : merged.slot;
    if (!decision.startFromCheckpoint) {
        return decision;
    }

    std::string loadError;
    decision.snapshot = store.Load(decision.slot, &loadError);
    if (!decision.snapshot.has_value() && !loadError.empty() && error != nullptr) {
        *error = loadError;
    }
    return decision;
}

} // namespace ri::world
