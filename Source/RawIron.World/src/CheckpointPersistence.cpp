#include "RawIron/World/CheckpointPersistence.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ri::world {
namespace {

constexpr int kCheckpointFormatVersion = 2;
constexpr std::uintmax_t kMaxCheckpointFileBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaxCheckpointLineBytes = 1024U * 1024U;
constexpr std::size_t kMaxCheckpointCollectionItems = 10000U;
constexpr std::size_t kMaxCheckpointRecords = 100000U;
constexpr std::size_t kMaxSlotBytes = 512U;
constexpr std::size_t kMaxCheckpointQueryBytes = 64U * 1024U;

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
    constexpr double kFloatMax = static_cast<double>(std::numeric_limits<float>::max());
    if (std::any_of(values.begin(), values.end(), [kFloatMax](const double value) {
            return std::fabs(value) > kFloatMax;
        })) {
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
    stream << std::setprecision(std::numeric_limits<float>::max_digits10);
    stream << value.x << "," << value.y << "," << value.z;
    return stream.str();
}

int HexToInt(const char ch) noexcept {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

bool IsAsciiUnreserved(const unsigned char byte) noexcept {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
        || (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.' || byte == '~';
}

std::optional<std::string> PercentDecode(std::string_view value,
                                         const bool plusAsSpace,
                                         const bool requireEncodedReserved = false) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (plusAsSpace && c == '+') {
            decoded.push_back(' ');
            continue;
        }
        if (c == '%') {
            if (i + 2U >= value.size()) {
                return std::nullopt;
            }
            const int hiValue = HexToInt(value[i + 1U]);
            const int loValue = HexToInt(value[i + 2U]);
            if (hiValue < 0 || loValue < 0) {
                return std::nullopt;
            }
            const char decodedByte = static_cast<char>((hiValue << 4) | loValue);
            if (decodedByte == '\0') {
                return std::nullopt;
            }
            decoded.push_back(decodedByte);
            i += 2U;
            continue;
        }
        if (requireEncodedReserved && !IsAsciiUnreserved(static_cast<unsigned char>(c))) {
            return std::nullopt;
        }
        decoded.push_back(c);
    }
    return decoded;
}

std::string PercentEncode(std::string_view value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (const unsigned char byte : value) {
        if (IsAsciiUnreserved(byte)) {
            encoded.push_back(static_cast<char>(byte));
        } else {
            encoded.push_back('%');
            encoded.push_back(kHex[(byte >> 4U) & 0x0FU]);
            encoded.push_back(kHex[byte & 0x0FU]);
        }
    }
    return encoded;
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

std::string JoinEncodedList(const std::vector<std::string>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0U) stream << ',';
        stream << PercentEncode(values[index]);
    }
    return stream.str();
}

std::optional<std::vector<std::string>> SplitEncodedList(std::string_view input) {
    std::vector<std::string> values;
    std::unordered_set<std::string> seen;
    if (input.empty()) {
        return values;
    }
    std::size_t start = 0U;
    while (start <= input.size()) {
        const std::size_t comma = input.find(',', start);
        const std::size_t end = comma == std::string_view::npos ? input.size() : comma;
        const std::optional<std::string> decoded = PercentDecode(input.substr(start, end - start), false, true);
        if (!decoded.has_value() || decoded->empty() || values.size() >= kMaxCheckpointCollectionItems
            || !seen.insert(*decoded).second) {
            return std::nullopt;
        }
        values.push_back(*decoded);
        if (comma == std::string_view::npos) break;
        start = comma + 1U;
    }
    return values;
}

std::optional<bool> ParseBool(std::string_view value) {
    const std::string lowered = [&]() {
        std::string v = Trim(std::string(value));
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return v;
    }();
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") return true;
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") return false;
    return std::nullopt;
}

bool Vec3Finite(const ri::math::Vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

std::uint64_t Fnv1a64(std::string_view value) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string CanonicalSlotName(std::string_view slot) {
    if (slot.empty()) slot = "autosave";
    std::string encoded = PercentEncode(slot);
    std::string lowered = encoded;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    const bool reserved = lowered == "con" || lowered == "prn" || lowered == "aux" || lowered == "nul"
        || (lowered.size() == 4U && (lowered.rfind("com", 0) == 0U || lowered.rfind("lpt", 0) == 0U)
            && lowered[3] >= '1' && lowered[3] <= '9');
    const bool containsUppercase = std::any_of(slot.begin(), slot.end(), [](unsigned char ch) {
        return ch >= 'A' && ch <= 'Z';
    });
    if (reserved) encoded.insert(encoded.begin(), '_');
    if (containsUppercase) {
        std::ostringstream suffix;
        suffix << '_' << std::hex << std::setw(16) << std::setfill('0') << Fnv1a64(slot);
        encoded += suffix.str();
    }
    if (encoded.size() > 120U) {
        std::ostringstream suffix;
        suffix << '_' << std::hex << std::setw(16) << std::setfill('0') << Fnv1a64(slot);
        encoded.resize(120U - suffix.str().size());
        encoded += suffix.str();
    }
    return encoded;
}

std::string LegacySlotName(std::string_view slot) {
    std::string safeSlot(slot.empty() ? "autosave" : slot);
    for (char& ch : safeSlot) {
        const bool valid = std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '_';
        if (!valid) ch = '_';
    }
    return safeSlot;
}

bool LinesWithinLimit(std::string_view text) noexcept {
    std::size_t lineStart = 0U;
    while (lineStart < text.size()) {
        const std::size_t newline = text.find('\n', lineStart);
        const std::size_t lineEnd = newline == std::string_view::npos ? text.size() : newline;
        if (lineEnd - lineStart > kMaxCheckpointLineBytes) return false;
        if (newline == std::string_view::npos) break;
        lineStart = newline + 1U;
    }
    return true;
}

std::filesystem::path TemporaryCheckpointPath(const std::filesystem::path& targetPath) {
    static std::atomic<std::uint64_t> sequence{0U};
    const std::uint64_t stamp = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::path temporary = targetPath;
    temporary += ".tmp." + std::to_string(stamp) + "." + std::to_string(sequence.fetch_add(1U));
    return temporary;
}

} // namespace

FileCheckpointStore::FileCheckpointStore(std::filesystem::path rootDirectory)
    : rootDirectory_(std::move(rootDirectory)) {}

bool FileCheckpointStore::Save(const RuntimeCheckpointSnapshot& snapshot, std::string* error) const {
    if (error != nullptr) error->clear();
    const std::string normalizedSlot = snapshot.slot.empty() ? std::string("autosave") : snapshot.slot;
    if (normalizedSlot.size() > kMaxSlotBytes) {
        if (error != nullptr) *error = "Checkpoint slot exceeds the maximum length.";
        return false;
    }
    if (const std::optional<std::string> validationError =
            ri::validation::ValidateCheckpointState(snapshot.state, "checkpoint");
        validationError.has_value()) {
        if (error != nullptr) *error = *validationError;
        return false;
    }
    const ri::validation::RuntimeCheckpointState parsedState = ri::validation::ParseStoredCheckpointState(snapshot.state);
    if (const std::optional<std::string> validationError = ri::validation::ValidateCheckpointState(parsedState, "checkpoint");
        validationError.has_value()) {
        if (error != nullptr) {
            *error = *validationError;
        }
        return false;
    }
    if (parsedState.flags.size() > kMaxCheckpointCollectionItems
        || parsedState.eventIds.size() > kMaxCheckpointCollectionItems
        || parsedState.values.size() > kMaxCheckpointCollectionItems) {
        if (error != nullptr) *error = "Checkpoint collection exceeds the safety limit.";
        return false;
    }
    if ((snapshot.playerPosition.has_value() && !Vec3Finite(*snapshot.playerPosition))
        || (snapshot.playerRotation.has_value() && !Vec3Finite(*snapshot.playerRotation))) {
        if (error != nullptr) *error = "Checkpoint player transform must contain finite values.";
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

    std::ostringstream serialized;
    serialized << "version=" << kCheckpointFormatVersion << "\n";
    serialized << "slot=" << PercentEncode(normalizedSlot) << "\n";
    serialized << "level=" << PercentEncode(parsedState.level.value_or("")) << "\n";
    serialized << "checkpointId=" << PercentEncode(parsedState.checkpointId.value_or("")) << "\n";
    serialized << "flags=" << JoinEncodedList(parsedState.flags) << "\n";
    serialized << "eventIds=" << JoinEncodedList(parsedState.eventIds) << "\n";
    std::vector<std::pair<std::string, double>> sortedValues(parsedState.values.begin(), parsedState.values.end());
    std::sort(sortedValues.begin(), sortedValues.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });
    serialized << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (const auto& [key, number] : sortedValues) {
        serialized << "value:" << PercentEncode(key) << "=" << number << "\n";
    }
    if (snapshot.playerPosition.has_value()) {
        serialized << "playerPosition=" << ToVec3String(*snapshot.playerPosition) << "\n";
    }
    if (snapshot.playerRotation.has_value()) {
        serialized << "playerRotation=" << ToVec3String(*snapshot.playerRotation) << "\n";
    }
    const std::string bytes = serialized.str();
    if (bytes.size() > kMaxCheckpointFileBytes || !LinesWithinLimit(bytes)) {
        if (error != nullptr) *error = "Checkpoint data exceeds the file safety limit.";
        return false;
    }

    const std::filesystem::path targetPath = SlotPath(normalizedSlot);
    const std::filesystem::path temporaryPath = TemporaryCheckpointPath(targetPath);
    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        if (error != nullptr) {
            *error = "Failed to open checkpoint file for write.";
        }
        return false;
    }
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.flush();
    if (!output.good()) {
        output.close();
        std::error_code cleanupError;
        std::filesystem::remove(temporaryPath, cleanupError);
        if (error != nullptr) {
            *error = "Failed to write checkpoint file.";
        }
        return false;
    }
    output.close();

#if defined(_WIN32)
    if (!MoveFileExW(temporaryPath.c_str(), targetPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code cleanupError;
        std::filesystem::remove(temporaryPath, cleanupError);
        if (error != nullptr) *error = "Failed to atomically replace checkpoint file.";
        return false;
    }
#else
    std::error_code renameError;
    std::filesystem::rename(temporaryPath, targetPath, renameError);
    if (renameError) {
        std::error_code cleanupError;
        std::filesystem::remove(temporaryPath, cleanupError);
        if (error != nullptr) *error = "Failed to atomically replace checkpoint file: " + renameError.message();
        return false;
    }
#endif
    return true;
}

std::optional<RuntimeCheckpointSnapshot> FileCheckpointStore::Load(std::string_view slot, std::string* error) const {
    if (error != nullptr) error->clear();
    const std::string normalizedSlot = slot.empty() ? std::string("autosave") : std::string(slot);
    const auto fail = [error](std::string message) -> std::optional<RuntimeCheckpointSnapshot> {
        if (error != nullptr) *error = std::move(message);
        return std::nullopt;
    };
    if (normalizedSlot.size() > kMaxSlotBytes) {
        return fail("Checkpoint slot exceeds the maximum length.");
    }

    std::filesystem::path inputPath = SlotPath(normalizedSlot);
    if (!std::filesystem::exists(inputPath)) {
        const std::filesystem::path legacyPath = rootDirectory_ / (LegacySlotName(normalizedSlot) + ".checkpoint");
        if (legacyPath != inputPath && std::filesystem::exists(legacyPath)) {
            inputPath = legacyPath;
        }
    }
    std::error_code sizeError;
    const std::uintmax_t fileSize = std::filesystem::file_size(inputPath, sizeError);
    if (sizeError) {
        return fail("Checkpoint file is missing or unreadable.");
    }
    if (fileSize > kMaxCheckpointFileBytes) {
        return fail("Checkpoint file exceeds the safety limit.");
    }

    std::ifstream input(inputPath, std::ios::binary);
    if (!input.is_open()) {
        return fail("Failed to open checkpoint file for read.");
    }

    RuntimeCheckpointSnapshot snapshot{};
    snapshot.slot = normalizedSlot;
    std::string line;
    if (!std::getline(input, line) || line.size() > kMaxCheckpointLineBytes || line.rfind("version=", 0) != 0) {
        return fail("Checkpoint header is missing or malformed.");
    }
    int formatVersion = 0;
    {
        const std::string versionText = line.substr(std::string("version=").size());
        char* end = nullptr;
        const long parsed = std::strtol(versionText.c_str(), &end, 10);
        if (end == versionText.c_str() || *end != '\0' || (parsed != 1 && parsed != kCheckpointFormatVersion)) {
            return fail("Checkpoint format version is unsupported.");
        }
        formatVersion = static_cast<int>(parsed);
    }

    std::unordered_set<std::string> seenScalarKeys;
    std::unordered_set<std::string> seenValueKeys;
    std::string storedSlot;
    const auto decodeField = [formatVersion](std::string_view value) -> std::optional<std::string> {
        return formatVersion >= 2 ? PercentDecode(value, false, true)
                                  : std::optional<std::string>(Trim(std::string(value)));
    };
    std::size_t recordCount = 0U;
    while (std::getline(input, line)) {
        if (++recordCount > kMaxCheckpointRecords) {
            return fail("Checkpoint contains too many records.");
        }
        if (line.size() > kMaxCheckpointLineBytes) {
            return fail("Checkpoint line exceeds the safety limit.");
        }
        const std::size_t equalsIndex = line.find('=');
        if (equalsIndex == std::string::npos) {
            return fail("Checkpoint contains a malformed record.");
        }
        const std::string key = Trim(line.substr(0, equalsIndex));
        const std::string rawValue = line.substr(equalsIndex + 1U);
        if (key.empty()) {
            return fail("Checkpoint contains an empty record key.");
        }
        if (key == "version") {
            return fail("Checkpoint contains a duplicate version record.");
        }
        if (key == "slot" || key == "level" || key == "checkpointId" || key == "flags"
            || key == "eventIds" || key == "playerPosition" || key == "playerRotation") {
            if (!seenScalarKeys.insert(key).second) {
                return fail("Checkpoint contains duplicate record '" + key + "'.");
            }
        }
        if (key == "slot") {
            const std::optional<std::string> decoded = decodeField(rawValue);
            if (!decoded.has_value()) return fail("Checkpoint slot encoding is malformed.");
            storedSlot = *decoded;
            continue;
        }
        if (key == "level") {
            const std::optional<std::string> decoded = decodeField(rawValue);
            if (!decoded.has_value()) return fail("Checkpoint level encoding is malformed.");
            snapshot.state.level = *decoded;
            continue;
        }
        if (key == "checkpointId") {
            const std::optional<std::string> decoded = decodeField(rawValue);
            if (!decoded.has_value()) return fail("Checkpoint id encoding is malformed.");
            if (!decoded->empty()) snapshot.state.checkpointId = *decoded;
            continue;
        }
        if (key == "flags") {
            if (formatVersion >= 2) {
                const auto decoded = SplitEncodedList(rawValue);
                if (!decoded.has_value()) return fail("Checkpoint flags encoding is malformed.");
                snapshot.state.flags = *decoded;
            } else {
                snapshot.state.flags = SplitCsv(rawValue);
            }
            continue;
        }
        if (key == "eventIds") {
            if (formatVersion >= 2) {
                const auto decoded = SplitEncodedList(rawValue);
                if (!decoded.has_value()) return fail("Checkpoint event id encoding is malformed.");
                snapshot.state.eventIds = *decoded;
            } else {
                snapshot.state.eventIds = SplitCsv(rawValue);
            }
            continue;
        }
        if (key == "playerPosition") {
            ri::math::Vec3 parsed{};
            if (!ParseVec3(rawValue, parsed)) return fail("Checkpoint player position is malformed.");
            snapshot.playerPosition = parsed;
            continue;
        }
        if (key == "playerRotation") {
            ri::math::Vec3 parsed{};
            if (!ParseVec3(rawValue, parsed)) return fail("Checkpoint player rotation is malformed.");
            snapshot.playerRotation = parsed;
            continue;
        }
        if (key.rfind("value:", 0) == 0) {
            const std::string encodedKey = key.substr(std::string("value:").size());
            const std::optional<std::string> decodedKey = decodeField(encodedKey);
            if (!decodedKey.has_value() || decodedKey->empty()) {
                return fail("Checkpoint value key encoding is malformed.");
            }
            if (seenValueKeys.size() >= kMaxCheckpointCollectionItems || !seenValueKeys.insert(*decodedKey).second) {
                return fail("Checkpoint contains duplicate or excessive value records.");
            }
            double number = 0.0;
            if (!ParseDouble(rawValue, number)) return fail("Checkpoint numeric value is malformed.");
            snapshot.state.values[*decodedKey] = number;
        }
    }
    if (input.bad()) {
        return fail("Failed while reading checkpoint file.");
    }
    if (!seenScalarKeys.contains("slot") || !seenScalarKeys.contains("level")) {
        return fail("Checkpoint is missing required slot or level records.");
    }
    if (storedSlot != normalizedSlot && !(formatVersion == 1 && storedSlot.empty() && normalizedSlot == "autosave")) {
        return fail("Checkpoint slot does not match the requested slot.");
    }
    if (snapshot.state.flags.size() > kMaxCheckpointCollectionItems
        || snapshot.state.eventIds.size() > kMaxCheckpointCollectionItems) {
        return fail("Checkpoint collection exceeds the safety limit.");
    }

    snapshot.state = ri::validation::ParseStoredCheckpointState(snapshot.state);
    if (const std::optional<std::string> validationError = ri::validation::ValidateCheckpointState(snapshot.state, "checkpoint");
        validationError.has_value()) {
        return fail(*validationError);
    }
    return snapshot;
}

bool FileCheckpointStore::Clear(std::string_view slot, std::string* error) const {
    if (error != nullptr) error->clear();
    const std::string normalizedSlot = slot.empty() ? std::string("autosave") : std::string(slot);
    if (normalizedSlot.size() > kMaxSlotBytes) {
        if (error != nullptr) *error = "Checkpoint slot exceeds the maximum length.";
        return false;
    }
    std::error_code removalError;
    std::filesystem::remove(SlotPath(normalizedSlot), removalError);
    if (removalError) {
        if (error != nullptr) {
            *error = "Failed to clear checkpoint file: " + removalError.message();
        }
        return false;
    }
    const std::filesystem::path legacyPath = rootDirectory_ / (LegacySlotName(normalizedSlot) + ".checkpoint");
    if (legacyPath != SlotPath(normalizedSlot)) {
        std::filesystem::remove(legacyPath, removalError);
        if (removalError) {
            if (error != nullptr) *error = "Failed to clear legacy checkpoint file: " + removalError.message();
            return false;
        }
    }
    return true;
}

std::filesystem::path FileCheckpointStore::SlotPath(std::string_view slot) const {
    return rootDirectory_ / (CanonicalSlotName(slot) + ".checkpoint");
}

CheckpointStartupOptions ParseCheckpointStartupOptions(std::string_view queryString) {
    CheckpointStartupOptions options{};
    if (queryString.size() > kMaxCheckpointQueryBytes) {
        return options;
    }
    std::string raw(queryString);
    if (!raw.empty() && raw.front() == '?') {
        raw.erase(raw.begin());
    }
    std::stringstream stream(raw);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        const std::size_t equalsIndex = pair.find('=');
        const std::optional<std::string> key = PercentDecode(Trim(pair.substr(0, equalsIndex)), true);
        const std::optional<std::string> value = equalsIndex == std::string::npos
            ? std::optional<std::string>(std::string{})
            : PercentDecode(Trim(pair.substr(equalsIndex + 1U)), true);
        if (!key.has_value() || !value.has_value()) {
            continue;
        }
        if (*key == "startFromCheckpoint") {
            if (const std::optional<bool> parsed = ParseBool(*value); parsed.has_value()) {
                options.startFromCheckpoint = *parsed;
            }
        } else if (*key == "checkpointSlot") {
            const std::string decodedSlot = Trim(*value);
            if (!decodedSlot.empty() && decodedSlot.size() <= kMaxSlotBytes) {
                options.slot = decodedSlot;
                options.slotProvided = true;
            }
        }
    }
    return options;
}

CheckpointStartupDecision ResolveCheckpointStartupDecision(const CheckpointStartupOptions& options,
                                                           const FileCheckpointStore& store,
                                                           std::string* error) {
    if (error != nullptr) error->clear();
    CheckpointStartupOptions merged = options;
    if (merged.queryString.has_value()) {
        const CheckpointStartupOptions parsedFromQuery = ParseCheckpointStartupOptions(*merged.queryString);
        merged.startFromCheckpoint = merged.startFromCheckpoint || parsedFromQuery.startFromCheckpoint;
        if (parsedFromQuery.slotProvided) {
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
