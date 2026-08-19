#include "RawIron/Content/AssetDocument.h"
#include "RawIron/Content/AssetPackageManifest.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <process.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;

struct ZipEntry {
    std::string name{};
    std::vector<std::byte> data{};
    std::uint16_t flags = 0U;
    std::uint16_t method = 0U;
    std::uint16_t madeBy = 20U;
    std::uint32_t externalAttributes = 0U;
    std::optional<std::uint32_t> declaredCompressed{};
    std::optional<std::uint32_t> declaredExpanded{};
    std::optional<std::uint32_t> declaredCrc{};
};

struct CentralRecord {
    ZipEntry entry{};
    std::uint32_t localOffset = 0U;
};

void Put16(std::ostream& output, const std::uint16_t value) {
    output.put(static_cast<char>(value & 0xFFU));
    output.put(static_cast<char>((value >> 8U) & 0xFFU));
}

void Put32(std::ostream& output, const std::uint32_t value) {
    Put16(output, static_cast<std::uint16_t>(value & 0xFFFFU));
    Put16(output, static_cast<std::uint16_t>(value >> 16U));
}

std::uint32_t Crc32(const std::vector<std::byte>& bytes) {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::byte byte : bytes) {
        crc ^= std::to_integer<unsigned char>(byte);
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? 0xEDB88320U : 0U);
        }
    }
    return ~crc;
}

std::vector<std::byte> Bytes(const std::string_view text) {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const unsigned char value : text) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

void WriteZip(const fs::path& path, const std::vector<ZipEntry>& entries) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create ZIP fixture");
    }
    std::vector<CentralRecord> central;
    central.reserve(entries.size());
    for (const ZipEntry& entry : entries) {
        const std::uint64_t localOffset = static_cast<std::uint64_t>(output.tellp());
        if (localOffset > 0xFFFFFFFFULL || entry.name.size() > 0xFFFFU) {
            throw std::runtime_error("ZIP fixture exceeds classic ZIP bounds");
        }
        const std::uint32_t compressed = entry.declaredCompressed.value_or(
            static_cast<std::uint32_t>(entry.data.size()));
        const std::uint32_t expanded = entry.declaredExpanded.value_or(
            static_cast<std::uint32_t>(entry.data.size()));
        const std::uint32_t crc = entry.declaredCrc.value_or(Crc32(entry.data));
        Put32(output, 0x04034b50U);
        Put16(output, 20U);
        Put16(output, entry.flags);
        Put16(output, entry.method);
        Put16(output, 0U);
        Put16(output, 0U);
        Put32(output, (entry.flags & 0x0008U) != 0U ? 0U : crc);
        Put32(output, (entry.flags & 0x0008U) != 0U ? 0U : compressed);
        Put32(output, (entry.flags & 0x0008U) != 0U ? 0U : expanded);
        Put16(output, static_cast<std::uint16_t>(entry.name.size()));
        Put16(output, 0U);
        output.write(entry.name.data(), static_cast<std::streamsize>(entry.name.size()));
        output.write(reinterpret_cast<const char*>(entry.data.data()),
                     static_cast<std::streamsize>(entry.data.size()));
        if ((entry.flags & 0x0008U) != 0U) {
            Put32(output, 0x08074b50U);
            Put32(output, crc);
            Put32(output, compressed);
            Put32(output, expanded);
        }
        central.push_back({entry, static_cast<std::uint32_t>(localOffset)});
    }
    const std::uint64_t centralOffset64 = static_cast<std::uint64_t>(output.tellp());
    for (const CentralRecord& record : central) {
        const ZipEntry& entry = record.entry;
        const std::uint32_t compressed = entry.declaredCompressed.value_or(
            static_cast<std::uint32_t>(entry.data.size()));
        const std::uint32_t expanded = entry.declaredExpanded.value_or(
            static_cast<std::uint32_t>(entry.data.size()));
        const std::uint32_t crc = entry.declaredCrc.value_or(Crc32(entry.data));
        Put32(output, 0x02014b50U);
        Put16(output, entry.madeBy);
        Put16(output, 20U);
        Put16(output, entry.flags);
        Put16(output, entry.method);
        Put16(output, 0U);
        Put16(output, 0U);
        Put32(output, crc);
        Put32(output, compressed);
        Put32(output, expanded);
        Put16(output, static_cast<std::uint16_t>(entry.name.size()));
        Put16(output, 0U);
        Put16(output, 0U);
        Put16(output, 0U);
        Put16(output, 0U);
        Put32(output, entry.externalAttributes);
        Put32(output, record.localOffset);
        output.write(entry.name.data(), static_cast<std::streamsize>(entry.name.size()));
    }
    const std::uint64_t centralEnd64 = static_cast<std::uint64_t>(output.tellp());
    if (centralOffset64 > 0xFFFFFFFFULL || centralEnd64 - centralOffset64 > 0xFFFFFFFFULL
        || entries.size() > 0xFFFFU) {
        throw std::runtime_error("ZIP fixture central directory exceeds classic ZIP bounds");
    }
    Put32(output, 0x06054b50U);
    Put16(output, 0U);
    Put16(output, 0U);
    Put16(output, static_cast<std::uint16_t>(entries.size()));
    Put16(output, static_cast<std::uint16_t>(entries.size()));
    Put32(output, static_cast<std::uint32_t>(centralEnd64 - centralOffset64));
    Put32(output, static_cast<std::uint32_t>(centralOffset64));
    Put16(output, 0U);
    if (!output) {
        throw std::runtime_error("could not finish ZIP fixture");
    }
}

void Patch16(const fs::path& path, const std::uint64_t offset, const std::uint16_t value) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    file.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    Put16(file, value);
    if (!file) {
        throw std::runtime_error("could not patch ZIP fixture");
    }
}

void Patch32(const fs::path& path, const std::uint64_t offset, const std::uint32_t value) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    file.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    Put32(file, value);
    if (!file) {
        throw std::runtime_error("could not patch ZIP fixture");
    }
}

struct BitWriter {
    std::vector<std::byte> bytes{};
    unsigned bitOffset = 0U;

    void Put(const std::uint32_t value, const unsigned count) {
        for (unsigned bit = 0U; bit < count; ++bit) {
            if (bitOffset == 0U) {
                bytes.push_back(std::byte{0U});
            }
            if ((value & (1U << bit)) != 0U) {
                bytes.back() |= static_cast<std::byte>(1U << bitOffset);
            }
            bitOffset = (bitOffset + 1U) & 7U;
        }
    }
};

std::uint32_t ReverseBits(std::uint32_t value, const unsigned count) {
    std::uint32_t result = 0U;
    for (unsigned bit = 0U; bit < count; ++bit) {
        result = (result << 1U) | ((value >> bit) & 1U);
    }
    return result;
}

void PutFixedLiteral(BitWriter& writer, const unsigned symbol) {
    if (symbol <= 143U) {
        writer.Put(ReverseBits(0x30U + symbol, 8U), 8U);
    } else if (symbol <= 255U) {
        writer.Put(ReverseBits(0x190U + symbol - 144U, 9U), 9U);
    } else if (symbol <= 279U) {
        writer.Put(ReverseBits(symbol - 256U, 7U), 7U);
    } else {
        writer.Put(ReverseBits(0xC0U + symbol - 280U, 8U), 8U);
    }
}

std::vector<std::byte> FixedDeflate(const std::string_view text) {
    BitWriter writer;
    writer.Put(1U, 1U); // BFINAL
    writer.Put(1U, 2U); // BTYPE=fixed Huffman (01, least-significant bit first)
    for (const unsigned char value : text) {
        PutFixedLiteral(writer, value);
    }
    PutFixedLiteral(writer, 256U);
    return writer.bytes;
}

std::string ReadText(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

struct ProcessResult {
    bool launched = false;
    int exitCode = -1;
    std::string output{};
};

#if defined(_WIN32)
std::wstring QuoteWindowsArgument(const fs::path& path) {
    std::wstring result = L"\"";
    for (const wchar_t value : path.native()) {
        if (value == L'\"') {
            result += L"\\\"";
        } else {
            result.push_back(value);
        }
    }
    result += L"\"";
    return result;
}
#endif

ProcessResult RunTool(const fs::path& tool, const std::vector<fs::path>& arguments, const fs::path& log) {
#if defined(_WIN32)
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    const HANDLE logHandle = CreateFileW(
        log.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, &security,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logHandle == INVALID_HANDLE_VALUE) {
        return {};
    }
    std::wstring command = QuoteWindowsArgument(tool);
    for (const fs::path& argument : arguments) {
        command += L' ';
        command += QuoteWindowsArgument(argument);
    }
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = logHandle;
    startup.hStdError = logHandle;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const BOOL launched = CreateProcessW(
        tool.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, nullptr, &startup, &process);
    if (launched == FALSE) {
        CloseHandle(logHandle);
        return {};
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0U;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(logHandle);
    return {true, static_cast<int>(exitCode), ReadText(log)};
#else
    const pid_t child = fork();
    if (child < 0) {
        return {};
    }
    if (child == 0) {
        const int descriptor = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (descriptor < 0) {
            _exit(126);
        }
        dup2(descriptor, STDOUT_FILENO);
        dup2(descriptor, STDERR_FILENO);
        close(descriptor);
        std::vector<std::string> texts;
        texts.reserve(arguments.size() + 1U);
        texts.push_back(tool.string());
        for (const fs::path& argument : arguments) {
            texts.push_back(argument.string());
        }
        std::vector<char*> pointers;
        pointers.reserve(texts.size() + 1U);
        for (std::string& text : texts) {
            pointers.push_back(text.data());
        }
        pointers.push_back(nullptr);
        execv(tool.c_str(), pointers.data());
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status)) {
        return {};
    }
    return {WEXITSTATUS(status) != 127, WEXITSTATUS(status), ReadText(log)};
#endif
}

std::vector<std::string> StagingRoots() {
    std::vector<std::string> roots;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(fs::temp_directory_path(), error)) {
        if (entry.path().filename().string().starts_with("RawIronRipak.extract.")) {
            roots.push_back(entry.path().filename().string());
        }
    }
    std::sort(roots.begin(), roots.end());
    return roots;
}

int Fail(const std::string& message) {
    std::cerr << "RipakArchiveSecuritySmoke: " << message << '\n';
    return EXIT_FAILURE;
}

} // namespace

int main(const int argc, const char* const* argv) {
    if (argc != 2) {
        return Fail("expected the ri_tool executable path");
    }
    const fs::path tool = fs::path(argv[1]);
    if (!fs::is_regular_file(tool)) {
        return Fail("ri_tool executable does not exist");
    }
#if defined(_WIN32)
    const std::uint64_t processId = static_cast<std::uint64_t>(_getpid());
#else
    const std::uint64_t processId = static_cast<std::uint64_t>(getpid());
#endif
    const std::uint64_t tick = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path root = fs::temp_directory_path()
        / ("RawIronRipakSecuritySmoke." + std::to_string(processId) + "." + std::to_string(tick));
    std::error_code error;
    if (!fs::create_directory(root, error) || error) {
        return Fail("could not create fixture root");
    }
    const std::vector<std::string> stagingBefore = StagingRoots();
    const auto cleanupAndFail = [&](const std::string& message) {
        std::error_code cleanupError;
        fs::remove_all(root, cleanupError);
        return Fail(message + (cleanupError ? "; cleanup: " + cleanupError.message() : ""));
    };

    const fs::path packageDirectory = root / "valid-package";
    fs::create_directories(packageDirectory / "assets", error);
    ri::content::AssetDocument document{};
    document.id = "rawiron.secure-ripak.payload";
    document.type = "data";
    document.displayName = "Secure ripak payload";
    document.sourcePath = "generated/security-smoke";
    document.payloadJson = R"({"secureRipak":true})";
    const fs::path assetPath = packageDirectory / "assets" / "payload.ri_asset.json";
    if (!ri::content::SaveAssetDocument(assetPath, document)) {
        return cleanupAndFail("could not create valid package asset");
    }
    ri::content::AssetPackageManifest manifest = ri::content::BuildAssetPackageManifest(
        packageDirectory, "rawiron.secure-ripak", "Secure ripak", "generated", "2026-08-03T00:00:00Z");
    manifest.packageVersion = "1.0.0";
    manifest.installScope = "either";
    if (manifest.assets.size() != 1U) {
        return cleanupAndFail("valid package fixture did not inventory one asset");
    }
    manifest.assets.front().installPath = "assets/secure-ripak/payload.ri_asset.json";
    const fs::path manifestPath = packageDirectory / "package.ri_package.json";
    if (!ri::content::SaveAssetPackageManifest(manifestPath, manifest)) {
        return cleanupAndFail("could not create valid package manifest");
    }
    const auto readBytes = [](const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        const std::string text{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        return Bytes(text);
    };
    const fs::path validArchive = root / "valid.ripak";
    WriteZip(validArchive, {
        {"assets/payload.ri_asset.json", readBytes(assetPath)},
        {"package.ri_package.json", readBytes(manifestPath)},
    });

    std::size_t logIndex = 0U;
    const auto invoke = [&](const fs::path& archive, const std::vector<fs::path>& suffix) {
        std::vector<fs::path> arguments{fs::path("--asset-package-validate"), archive};
        arguments.insert(arguments.end(), suffix.begin(), suffix.end());
        return RunTool(tool, arguments, root / ("tool-" + std::to_string(logIndex++) + ".log"));
    };
    const ProcessResult validation = invoke(validArchive, {});
    if (!validation.launched || validation.exitCode != 0
        || validation.output.find("RawIron asset package validated") == std::string::npos) {
        return cleanupAndFail("valid real .ripak did not validate: " + validation.output);
    }
    const fs::path descriptorArchive = root / "valid-descriptor.ripak";
    WriteZip(descriptorArchive, {
        {"assets/payload.ri_asset.json", readBytes(assetPath), 0x0008U},
        {"package.ri_package.json", readBytes(manifestPath), 0x0008U},
    });
    const ProcessResult descriptorValidation = invoke(descriptorArchive, {});
    if (!descriptorValidation.launched || descriptorValidation.exitCode != 0) {
        return cleanupAndFail("valid data-descriptor .ripak failed: " + descriptorValidation.output);
    }

    const fs::path installProject = root / "install-project";
    fs::create_directories(installProject, error);
    ProcessResult install = RunTool(
        tool,
        {fs::path("--asset-package-install"), validArchive, fs::path("--project"), installProject},
        root / ("tool-" + std::to_string(logIndex++) + ".log"));
    if (!install.launched || install.exitCode != 0
        || !fs::is_regular_file(installProject / "assets/secure-ripak/payload.ri_asset.json")) {
        return cleanupAndFail("valid .ripak install failed: " + install.output);
    }
    const fs::path importProject = root / "import-project";
    const fs::path importOutput = root / "mounted-output";
    fs::create_directories(importProject, error);
    ProcessResult import = RunTool(
        tool,
        {fs::path("--asset-package-import"), validArchive, fs::path("--project"), importProject,
         fs::path("--output-dir"), importOutput},
        root / ("tool-" + std::to_string(logIndex++) + ".log"));
    if (!import.launched || import.exitCode != 0
        || !fs::is_regular_file(importOutput / "assets/payload.ri_asset.json")) {
        return cleanupAndFail("valid .ripak import failed: " + import.output);
    }

    const std::string escapeName = "RawIronRipakEscape." + std::to_string(processId);
    const fs::path escapeTarget = fs::temp_directory_path() / escapeName;
    const std::vector<std::pair<std::string, std::vector<ZipEntry>>> rejected = {
        {"zip-slip", {{"../" + escapeName, Bytes("escape")}}},
        {"absolute", {{"/absolute.txt", Bytes("escape")}}},
        {"backslash", {{"assets\\escape.txt", Bytes("escape")}}},
        {"device", {{"CON/payload.txt", Bytes("escape")}}},
        {"case-collision", {{"Assets/payload.txt", Bytes("one")}, {"assets/PAYLOAD.txt", Bytes("two")}}},
        {"duplicate", {{"assets/same.txt", Bytes("one")}, {"assets/same.txt", Bytes("two")}}},
        {"prefix-collision", {{"assets/node", Bytes("file")}, {"assets/node/child.txt", Bytes("child")}}},
        {"encrypted", {{"assets/encrypted.txt", Bytes("cipher"), 0x0001U}}},
        {"unsupported-method", {{"assets/bzip.txt", Bytes("payload"), 0U, 12U}}},
        {"unix-symlink", {{"assets/link", Bytes("target"), 0U, 0U,
                           static_cast<std::uint16_t>((3U << 8U) | 20U),
                           static_cast<std::uint32_t>(0120777U << 16U)}}},
        {"per-file-budget", {{"assets/huge.bin", {}, 0U, 0U, 20U, 0U, 0U,
                              static_cast<std::uint32_t>(256ULL * 1024ULL * 1024ULL + 1ULL)}}},
        {"ratio-budget", {{"assets/ratio.bin", Bytes("x"), 0U, 8U, 20U, 0U, 1U, 201U}}},
        {"actual-output", {{"assets/actual.bin", FixedDeflate("AB"), 0U, 8U, 20U, 0U,
                            std::nullopt, 1U, Crc32(Bytes("AB"))}}},
        {"crc-corrupt", {{"assets/corrupt.bin", FixedDeflate("valid"), 0U, 8U, 20U, 0U,
                          std::nullopt, 5U, 0x12345678U}}},
        {"truncated-deflate", {{"assets/truncated.bin", {std::byte{0x73U}}, 0U, 8U, 20U, 0U,
                               std::nullopt, 5U, Crc32(Bytes("valid"))}}},
        {"trailing-deflate", [&]() {
            std::vector<std::byte> padded = FixedDeflate("AB");
            padded.push_back(std::byte{0x00U});
            padded.push_back(std::byte{0xFFU});
            return std::vector<ZipEntry>{{"assets/trailing.bin", std::move(padded), 0U, 8U, 20U, 0U,
                                          std::nullopt, 2U, Crc32(Bytes("AB"))}};
        }()},
        {"mid-extract-failure", {{"assets/first.txt", Bytes("first")},
                                 {"assets/second.bin", FixedDeflate("valid"), 0U, 8U, 20U, 0U,
                                  std::nullopt, 5U, 0x12345678U}}},
    };
    for (const auto& [label, entries] : rejected) {
        const fs::path archive = root / (label + ".ripak");
        WriteZip(archive, entries);
        const ProcessResult result = invoke(archive, {});
        if (!result.launched || result.exitCode == 0
            || result.output.find("Unsafe .ripak archive") == std::string::npos) {
            return cleanupAndFail(label + " archive was not rejected with a security diagnostic: " + result.output);
        }
        if (fs::exists(escapeTarget)) {
            return cleanupAndFail(label + " archive wrote outside its staging root");
        }
    }

#if defined(_WIN32)
    const fs::path unicodeCollision = root / "unicode-case-collision.ripak";
    WriteZip(unicodeCollision, {
        {"assets/\xC3\x84.txt", Bytes("one"), 0x0800U},
        {"assets/\xC3\xA4.txt", Bytes("two"), 0x0800U},
    });
    const ProcessResult unicodeResult = invoke(unicodeCollision, {});
    if (unicodeResult.exitCode == 0
        || unicodeResult.output.find("collides with another entry") == std::string::npos) {
        return cleanupAndFail("Windows Unicode case collision was not rejected: " + unicodeResult.output);
    }
#endif

    struct MutationCase {
        std::string label;
        std::uint64_t offset;
        std::uint32_t value;
        bool word;
    };
    const std::string mutationName = "assets/mutation.txt";
    const std::vector<MutationCase> mutations = {
        {"local-flags-mismatch", 6U, 0x0008U, true},
        {"local-method-mismatch", 8U, 8U, true},
        {"local-size-mismatch", 22U, 999U, false},
        {"local-name-mismatch", 30U, static_cast<std::uint32_t>('X'), true},
    };
    for (const MutationCase& mutation : mutations) {
        const fs::path archive = root / (mutation.label + ".ripak");
        WriteZip(archive, {{mutationName, Bytes("payload")}});
        if (mutation.label == "local-name-mismatch") {
            std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
            file.seekp(30, std::ios::beg);
            file.put('X');
        } else if (mutation.word) {
            Patch16(archive, mutation.offset, static_cast<std::uint16_t>(mutation.value));
        } else {
            Patch32(archive, mutation.offset, mutation.value);
        }
        const ProcessResult result = invoke(archive, {});
        if (result.exitCode == 0
            || result.output.find("Unsafe .ripak archive") == std::string::npos) {
            return cleanupAndFail(mutation.label + " was not rejected: " + result.output);
        }
    }

    const fs::path overlapArchive = root / "overlap.ripak";
    const ZipEntry overlapFirst{
        "assets/first.txt", Bytes("first"), 0U, 0U, 20U, 0U, 15U, 15U};
    const ZipEntry overlapSecond{"assets/second.txt", Bytes("second")};
    WriteZip(overlapArchive, {overlapFirst, overlapSecond});
    const ProcessResult overlapResult = invoke(overlapArchive, {});
    if (overlapResult.exitCode == 0
        || overlapResult.output.find("overlapping") == std::string::npos) {
        return cleanupAndFail("overlapping local entry regions were not rejected: " + overlapResult.output);
    }

    const fs::path centralOverlapArchive = root / "central-overlap.ripak";
    WriteZip(centralOverlapArchive, {{"assets/central.txt", Bytes("payload")}});
    const std::uint64_t centralFileBytes = fs::file_size(centralOverlapArchive);
    const std::uint64_t centralStart = 30U + std::string("assets/central.txt").size() + 7U;
    Patch32(centralOverlapArchive, centralFileBytes - 22U + 16U,
            static_cast<std::uint32_t>(centralStart - 1U));
    const ProcessResult centralOverlapResult = invoke(centralOverlapArchive, {});
    if (centralOverlapResult.exitCode == 0
        || centralOverlapResult.output.find("central directory bounds") == std::string::npos) {
        return cleanupAndFail("central-directory overlap was not rejected: " + centralOverlapResult.output);
    }

    const fs::path invalidDescriptorArchive = root / "invalid-descriptor.ripak";
    const ZipEntry descriptorEntry{"assets/descriptor.txt", Bytes("payload"), 0x0008U};
    WriteZip(invalidDescriptorArchive, {descriptorEntry});
    const std::uint64_t descriptorOffset = 30U + descriptorEntry.name.size() + descriptorEntry.data.size();
    Patch32(invalidDescriptorArchive, descriptorOffset + 4U, 0x12345678U);
    const ProcessResult invalidDescriptorResult = invoke(invalidDescriptorArchive, {});
    if (invalidDescriptorResult.exitCode == 0
        || invalidDescriptorResult.output.find("invalid data descriptor") == std::string::npos) {
        return cleanupAndFail("invalid data descriptor was not rejected: " + invalidDescriptorResult.output);
    }

    std::vector<ZipEntry> tooMany;
    tooMany.reserve(16385U);
    for (std::size_t index = 0U; index < 16385U; ++index) {
        tooMany.push_back({"entries/e" + std::to_string(index), {}});
    }
    const fs::path entriesArchive = root / "entry-budget.ripak";
    WriteZip(entriesArchive, tooMany);
    const ProcessResult entriesResult = invoke(entriesArchive, {});
    if (entriesResult.exitCode == 0
        || entriesResult.output.find("entry-count budget exceeded") == std::string::npos) {
        return cleanupAndFail("entry-count budget was not enforced: " + entriesResult.output);
    }

    std::vector<ZipEntry> totalBudgetEntries;
    for (std::size_t index = 0U; index < 5U; ++index) {
        totalBudgetEntries.push_back({
            "total/file" + std::to_string(index), {}, 0U, 0U, 20U, 0U,
            static_cast<std::uint32_t>(220ULL * 1024ULL * 1024ULL),
            static_cast<std::uint32_t>(220ULL * 1024ULL * 1024ULL)});
    }
    const fs::path totalBudgetArchive = root / "total-budget.ripak";
    WriteZip(totalBudgetArchive, totalBudgetEntries);
    const ProcessResult totalBudgetResult = invoke(totalBudgetArchive, {});
    if (totalBudgetResult.exitCode == 0
        || totalBudgetResult.output.find("total expanded-byte budget exceeded") == std::string::npos) {
        return cleanupAndFail("total expanded-byte budget was not enforced: " + totalBudgetResult.output);
    }

    const fs::path archiveBudget = root / "archive-budget.ripak";
    WriteZip(archiveBudget, {{"small", Bytes("x")}});
    fs::resize_file(archiveBudget, 512ULL * 1024ULL * 1024ULL + 1ULL, error);
    if (error) {
        return cleanupAndFail("could not create sparse archive-budget fixture: " + error.message());
    }
    const ProcessResult archiveResult = invoke(archiveBudget, {});
    if (archiveResult.exitCode == 0
        || archiveResult.output.find("archive-byte budget exceeded") == std::string::npos) {
        return cleanupAndFail("archive-byte budget was not enforced: " + archiveResult.output);
    }

    if (StagingRoots() != stagingBefore) {
        return cleanupAndFail("success or failure left an owned extraction staging directory behind");
    }
    fs::remove_all(root, error);
    if (error) {
        return Fail("fixture cleanup failed: " + error.message());
    }
    return EXIT_SUCCESS;
}
