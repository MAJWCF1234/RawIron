#include "RawIron/World/CheckpointPersistence.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
}

bool ReplaceFirst(std::string& text, const std::string& from, const std::string& to) {
    const std::size_t position = text.find(from);
    if (position == std::string::npos) return false;
    text.replace(position, from.size(), to);
    return true;
}

ri::world::RuntimeCheckpointSnapshot MakeSnapshot(std::string slot) {
    ri::world::RuntimeCheckpointSnapshot snapshot{};
    snapshot.slot = std::move(slot);
    snapshot.state.level = "level\n=one,%+";
    snapshot.state.checkpointId = "checkpoint\r\n=id";
    snapshot.state.flags = {"comma,value", "equals=value", "line\nbreak", "percent%plus+"};
    snapshot.state.eventIds = {"event,one", "event=two", "event\nthree"};
    snapshot.state.values = {{"value:=,\nkey", 1.0 / 3.0}, {"z-last", -123456.789012345}};
    snapshot.playerPosition = ri::math::Vec3{0.123456791f, -98765.4297f, 42.0000038f};
    snapshot.playerRotation = ri::math::Vec3{-179.999985f, 89.1234589f, 0.000012345678f};
    return snapshot;
}

bool SameVec3(const ri::math::Vec3& lhs, const ri::math::Vec3& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "RawIronCheckpointPersistenceSmoke";
    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    const ri::world::FileCheckpointStore store(root);
    std::string error;

    const std::string complexSlot = "slot/a?b,+%";
    const ri::world::RuntimeCheckpointSnapshot source = MakeSnapshot(complexSlot);
    if (!store.Save(source, &error) || !error.empty()) return EXIT_FAILURE;
    const std::optional<ri::world::RuntimeCheckpointSnapshot> loaded = store.Load(complexSlot, &error);
    if (!loaded.has_value() || !error.empty() || loaded->slot != complexSlot
        || loaded->state.level != source.state.level || loaded->state.checkpointId != source.state.checkpointId
        || loaded->state.flags != source.state.flags || loaded->state.eventIds != source.state.eventIds
        || loaded->state.values != source.state.values
        || !SameVec3(*loaded->playerPosition, *source.playerPosition)
        || !SameVec3(*loaded->playerRotation, *source.playerRotation)) {
        return EXIT_FAILURE;
    }

    // Delimiter-heavy generated strings exercise the encoder as a small deterministic property test.
    ri::world::RuntimeCheckpointSnapshot generated = MakeSnapshot("generated");
    generated.state.flags.clear();
    for (int index = 1; index <= 128; ++index) {
        generated.state.flags.push_back("item" + std::to_string(index) + "%,=+\n\t/?:" + static_cast<char>('A' + index % 26));
    }
    if (!store.Save(generated, &error)) return EXIT_FAILURE;
    const std::string deterministicFirst = ReadText(root / "generated.checkpoint");
    if (!store.Save(generated, &error)
        || ReadText(root / "generated.checkpoint") != deterministicFirst) return EXIT_FAILURE;
    const auto generatedLoaded = store.Load("generated", &error);
    if (!generatedLoaded.has_value() || generatedLoaded->state.flags != generated.state.flags) return EXIT_FAILURE;

    // Slots that collided under underscore replacement must now remain independent.
    ri::world::RuntimeCheckpointSnapshot slash = MakeSnapshot("a/b");
    slash.state.level = "slash";
    ri::world::RuntimeCheckpointSnapshot question = MakeSnapshot("a?b");
    question.state.level = "question";
    if (!store.Save(slash, &error) || !store.Save(question, &error)
        || store.Load("a/b", &error)->state.level != "slash"
        || store.Load("a?b", &error)->state.level != "question") {
        return EXIT_FAILURE;
    }

    ri::world::RuntimeCheckpointSnapshot uppercase = MakeSnapshot("Save");
    uppercase.state.level = "uppercase";
    ri::world::RuntimeCheckpointSnapshot lowercase = MakeSnapshot("save");
    lowercase.state.level = "lowercase";
    if (!store.Save(uppercase, &error) || !store.Save(lowercase, &error)
        || store.Load("Save", &error)->state.level != "uppercase"
        || store.Load("save", &error)->state.level != "lowercase") {
        return EXIT_FAILURE;
    }

    ri::world::RuntimeCheckpointSnapshot reserved = MakeSnapshot("CON");
    reserved.state.level = "reserved";
    if (!store.Save(reserved, &error) || store.Load("CON", &error)->state.level != "reserved") {
        return EXIT_FAILURE;
    }
    ri::world::RuntimeCheckpointSnapshot autosave = MakeSnapshot("");
    autosave.state.level = "autosave-level";
    if (!store.Save(autosave, &error) || store.Load("", &error)->slot != "autosave") return EXIT_FAILURE;

    ri::world::RuntimeCheckpointSnapshot invalid = MakeSnapshot("invalid");
    invalid.playerPosition->x = std::numeric_limits<float>::quiet_NaN();
    if (store.Save(invalid, &error) || error.empty()) return EXIT_FAILURE;
    invalid = MakeSnapshot(std::string(513U, 's'));
    if (store.Save(invalid, &error) || store.Load(invalid.slot, &error).has_value() || store.Clear(invalid.slot, &error)) {
        return EXIT_FAILURE;
    }
    invalid = MakeSnapshot("invalid-state");
    invalid.state.flags.push_back("");
    if (store.Save(invalid, &error) || error.empty()) return EXIT_FAILURE;
    invalid = MakeSnapshot("invalid-number");
    invalid.state.values["bad"] = std::numeric_limits<double>::infinity();
    if (store.Save(invalid, &error) || error.empty()) return EXIT_FAILURE;
    invalid = MakeSnapshot("oversized-line");
    invalid.state.level = std::string(400000U, '=');
    if (store.Save(invalid, &error) || error.empty()) return EXIT_FAILURE;

    // Legacy v1 remains readable.
    fs::create_directories(root);
    WriteText(root / "legacy.checkpoint",
              "version=1\nslot=legacy\nlevel=old-level\ncheckpointId=old-id\nflags=a,b\n"
              "eventIds=e1,e2\nvalue:score=1.5\nplayerPosition=1,2,3\n");
    const auto legacy = store.Load("legacy", &error);
    if (!legacy.has_value() || legacy->state.level != "old-level" || legacy->state.flags.size() != 2U
        || legacy->state.values.at("score") != 1.5 || legacy->playerPosition->z != 3.0f) {
        return EXIT_FAILURE;
    }

    ri::world::RuntimeCheckpointSnapshot corruptSource = MakeSnapshot("corrupt");
    corruptSource.state.level = "corrupt-level";
    if (!store.Save(corruptSource, &error)) return EXIT_FAILURE;
    const fs::path corruptPath = root / "corrupt.checkpoint";
    const std::string validText = ReadText(corruptPath);
    struct CorruptionCase {
        const char* name;
        std::function<void(std::string&)> mutate;
    };
    const std::vector<CorruptionCase> corruptions = {
        {"missing header", [](std::string& text) { text.erase(0U, text.find('\n') + 1U); }},
        {"unsupported version", [](std::string& text) { ReplaceFirst(text, "version=2", "version=99"); }},
        {"duplicate level", [](std::string& text) { text += "level=again\n"; }},
        {"malformed record", [](std::string& text) { text += "no-equals-here\n"; }},
        {"unknown record", [](std::string& text) { text += "injected=1\n"; }},
        {"invalid percent", [](std::string& text) { ReplaceFirst(text, "level=corrupt-level", "level=%Q0"); }},
        {"unescaped reserved byte", [](std::string& text) { ReplaceFirst(text, "level=corrupt-level", "level=bad=value"); }},
        {"duplicate list item", [](std::string& text) {
            const std::size_t start = text.find("flags=") + std::string("flags=").size();
            const std::size_t comma = text.find(',', start);
            const std::string first = text.substr(start, comma - start);
            text.insert(text.find('\n', start), "," + first);
        }},
        {"duplicate value", [](std::string& text) { text += "value:z-last=5\n"; }},
        {"bad vector", [](std::string& text) { ReplaceFirst(text, "playerPosition=", "playerPosition=1,2,3,4#"); }},
        {"slot mismatch", [](std::string& text) { ReplaceFirst(text, "slot=corrupt", "slot=other"); }},
        {"missing level", [](std::string& text) {
            const std::size_t start = text.find("level=");
            text.erase(start, text.find('\n', start) - start + 1U);
        }},
        {"nonfinite number", [](std::string& text) {
            const std::size_t start = text.find("value:");
            const std::size_t equals = text.find('=', start);
            text.replace(equals + 1U, text.find('\n', equals) - equals - 1U, "nan");
        }},
    };
    for (const CorruptionCase& corruption : corruptions) {
        std::string damaged = validText;
        corruption.mutate(damaged);
        WriteText(corruptPath, damaged);
        error.clear();
        if (store.Load("corrupt", &error).has_value() || error.empty()) return EXIT_FAILURE;
    }
    WriteText(corruptPath, std::string(16U * 1024U * 1024U + 1U, 'x'));
    if (store.Load("corrupt", &error).has_value() || error.empty()) return EXIT_FAILURE;

#if defined(_WIN32)
    {
        // Symlink/reparse destinations must be rejected on load and save.
        const fs::path victim = root / "victim-target.txt";
        WriteText(victim, "do-not-overwrite");
        const fs::path linkPath = root / "symlink.checkpoint";
        fs::remove(linkPath, cleanupError);
        const std::wstring link = linkPath.wstring();
        const std::wstring target = victim.wstring();
        if (CreateSymbolicLinkW(link.c_str(), target.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
            || CreateSymbolicLinkW(link.c_str(), target.c_str(), 0)) {
            error.clear();
            if (store.Load("symlink", &error).has_value() || error.empty()) return EXIT_FAILURE;
            ri::world::RuntimeCheckpointSnapshot overwrite = MakeSnapshot("symlink");
            overwrite.state.level = "overwrite-attempt";
            error.clear();
            if (store.Save(overwrite, &error) || error.empty()) return EXIT_FAILURE;
            if (ReadText(victim) != "do-not-overwrite") return EXIT_FAILURE;
            // Clear must unlink the reparse point without deleting the victim target.
            error.clear();
            if (!store.Clear("symlink", &error) || !error.empty()) return EXIT_FAILURE;
            if (fs::exists(linkPath) || ReadText(victim) != "do-not-overwrite") return EXIT_FAILURE;
            fs::remove(linkPath, cleanupError);
        }
        fs::remove(victim, cleanupError);

        const fs::path dirSlot = root / "directory.checkpoint";
        fs::create_directories(dirSlot, cleanupError);
        error.clear();
        if (store.Clear("directory", &error) || error.empty()) return EXIT_FAILURE;
        fs::remove_all(dirSlot, cleanupError);

        // Root directory junctions must fail closed before create_directories/ofstream divert.
        const fs::path outsideRoot = root / "checkpoint-outside";
        const fs::path aliasRoot = root / "checkpoint-alias-root";
        fs::create_directories(outsideRoot, cleanupError);
        fs::remove_all(aliasRoot, cleanupError);
        const bool aliasOk =
            CreateSymbolicLinkW(aliasRoot.c_str(),
                                outsideRoot.c_str(),
                                SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)
            || CreateSymbolicLinkW(aliasRoot.c_str(), outsideRoot.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY);
        if (aliasOk) {
            ri::world::FileCheckpointStore aliasStore(aliasRoot);
            ri::world::RuntimeCheckpointSnapshot aliasSnap = MakeSnapshot("junction");
            error.clear();
            if (aliasStore.Save(aliasSnap, &error) || error.empty()) return EXIT_FAILURE;
            error.clear();
            if (aliasStore.Load("junction", &error).has_value()) return EXIT_FAILURE;
            error.clear();
            if (aliasStore.Clear("junction", &error) || error.empty()) return EXIT_FAILURE;
            const fs::path outsideVictim = outsideRoot / "junction.checkpoint";
            if (fs::exists(outsideVictim, cleanupError)) return EXIT_FAILURE;
            fs::remove_all(aliasRoot, cleanupError);
            fs::remove_all(outsideRoot, cleanupError);
        }
    }
#endif
    // Unknown keys must fail closed (not silently ignored).
    WriteText(corruptPath, "version=2\nslot=corrupt\nlevel=ok\ninjected=1\n");
    error.clear();
    if (store.Load("corrupt", &error).has_value()
        || error.find("unknown") == std::string::npos) {
        return EXIT_FAILURE;
    }
    // Collection safety limit (10k values) must reject before unbounded growth.
    std::string excessiveValues = "version=2\nslot=corrupt\nlevel=ok\n";
    for (std::size_t index = 0; index <= 10000U; ++index) {
        excessiveValues += "value:v" + std::to_string(index) + "=1\n";
    }
    WriteText(corruptPath, excessiveValues);
    error.clear();
    if (store.Load("corrupt", &error).has_value()
        || error.find("excessive") == std::string::npos) {
        return EXIT_FAILURE;
    }

    const auto query = ri::world::ParseCheckpointStartupOptions(
        "?startFromCheckpoint=%20YES%20&checkpointSlot=slot%2Fwith%2Bplus");
    if (!query.startFromCheckpoint || !query.slotProvided || query.slot != "slot/with+plus") return EXIT_FAILURE;
    const auto invalidQuery = ri::world::ParseCheckpointStartupOptions(
        "?startFromCheckpoint=maybe&checkpointSlot=bad%Q0");
    if (invalidQuery.startFromCheckpoint || invalidQuery.slotProvided) return EXIT_FAILURE;
    const auto oversizedQuery = ri::world::ParseCheckpointStartupOptions(
        std::string(64U * 1024U + 1U, 'x') + "&startFromCheckpoint=true");
    if (oversizedQuery.startFromCheckpoint || oversizedQuery.slotProvided) return EXIT_FAILURE;

    ri::world::RuntimeCheckpointSnapshot manual = MakeSnapshot("manual");
    manual.state.level = "manual-level";
    if (!store.Save(manual, &error)) return EXIT_FAILURE;
    ri::world::CheckpointStartupOptions startup{};
    startup.startFromCheckpoint = true;
    startup.slot = "manual";
    startup.queryString = "?startFromCheckpoint=true";
    const auto decision = ri::world::ResolveCheckpointStartupDecision(startup, store, &error);
    if (decision.slot != "manual" || !decision.snapshot.has_value()
        || decision.snapshot->state.level != "manual-level") {
        return EXIT_FAILURE;
    }

    if (!store.Clear(complexSlot, &error) || store.Load(complexSlot, &error).has_value()) return EXIT_FAILURE;
    for (const fs::directory_entry& entry : fs::directory_iterator(root)) {
        if (entry.path().filename().string().find(".tmp.") != std::string::npos) return EXIT_FAILURE;
    }
    fs::remove_all(root, cleanupError);
    return EXIT_SUCCESS;
}
