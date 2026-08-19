#include "RawIron/Content/AssetDocument.h"
#include "RawIron/Content/AssetPackageManifest.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winioctl.h>
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;

int Fail(const std::string& message) {
    std::cerr << "PackageInstallTrustBoundarySmoke: " << message << '\n';
    return EXIT_FAILURE;
}

std::string ReadFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

std::vector<std::string> SnapshotTree(const fs::path& root) {
    std::vector<std::string> entries;
    if (!fs::exists(root)) {
        return entries;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root)) {
        const std::string kind = entry.is_directory() ? "D:" : "F:";
        entries.push_back(kind + entry.path().lexically_relative(root).generic_string());
    }
    std::sort(entries.begin(), entries.end());
    return entries;
}

#if defined(_WIN32)
struct MountPointReparseBuffer {
    DWORD tag;
    WORD dataLength;
    WORD reserved;
    WORD substituteNameOffset;
    WORD substituteNameLength;
    WORD printNameOffset;
    WORD printNameLength;
    wchar_t pathBuffer[(MAXIMUM_REPARSE_DATA_BUFFER_SIZE - 16U) / sizeof(wchar_t)];
};

static_assert(offsetof(MountPointReparseBuffer, pathBuffer) == 16U);
static_assert(sizeof(MountPointReparseBuffer) == MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
#endif

bool CreateDirectoryAlias(
    const fs::path& target,
    const fs::path& alias,
    std::string& issue) {
    issue.clear();
#if defined(_WIN32)
    std::error_code canonicalError;
    const fs::path canonicalTarget = fs::canonical(target, canonicalError);
    if (canonicalError || !fs::is_directory(canonicalTarget)) {
        issue = canonicalError
            ? "alias target cannot be canonicalized: " + canonicalError.message()
            : "alias target is not a directory";
        return false;
    }

    const std::wstring printName = canonicalTarget.native();
    std::wstring substituteName;
    if (printName.starts_with(L"\\\\?\\UNC\\")) {
        substituteName = L"\\??\\UNC\\" + printName.substr(8U);
    } else if (printName.starts_with(L"\\\\?\\")) {
        substituteName = L"\\??\\" + printName.substr(4U);
    } else if (printName.starts_with(L"\\\\")) {
        substituteName = L"\\??\\UNC\\" + printName.substr(2U);
    } else {
        substituteName = L"\\??\\" + printName;
    }

    const std::size_t substituteBytes = substituteName.size() * sizeof(wchar_t);
    const std::size_t printBytes = printName.size() * sizeof(wchar_t);
    const std::size_t pathBytes = substituteBytes + sizeof(wchar_t)
        + printBytes + sizeof(wchar_t);
    constexpr std::size_t mountPointFieldsBytes = 4U * sizeof(WORD);
    const std::size_t reparseDataLength = mountPointFieldsBytes + pathBytes;
    const std::size_t controlBufferLength = 8U + reparseDataLength;
    if (substituteBytes > std::numeric_limits<WORD>::max()
        || printBytes > std::numeric_limits<WORD>::max()
        || reparseDataLength > std::numeric_limits<WORD>::max()
        || controlBufferLength > MAXIMUM_REPARSE_DATA_BUFFER_SIZE) {
        issue = "alias target is too long for a Windows mount-point reparse buffer";
        return false;
    }

    if (CreateDirectoryW(alias.c_str(), nullptr) == FALSE) {
        issue = "alias directory creation failed: "
            + std::error_code(static_cast<int>(GetLastError()), std::system_category()).message();
        return false;
    }

    const HANDLE aliasHandle = CreateFileW(
        alias.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);
    if (aliasHandle == INVALID_HANDLE_VALUE) {
        const std::error_code handleError(
            static_cast<int>(GetLastError()), std::system_category());
        (void)RemoveDirectoryW(alias.c_str());
        issue = "alias directory could not be opened as a reparse point: " + handleError.message();
        return false;
    }

    MountPointReparseBuffer buffer{};
    buffer.tag = IO_REPARSE_TAG_MOUNT_POINT;
    buffer.dataLength = static_cast<WORD>(reparseDataLength);
    buffer.substituteNameOffset = 0U;
    buffer.substituteNameLength = static_cast<WORD>(substituteBytes);
    buffer.printNameOffset = static_cast<WORD>(substituteBytes + sizeof(wchar_t));
    buffer.printNameLength = static_cast<WORD>(printBytes);
    std::memcpy(buffer.pathBuffer, substituteName.data(), substituteBytes);
    buffer.pathBuffer[substituteName.size()] = L'\0';
    auto* printDestination = reinterpret_cast<wchar_t*>(
        reinterpret_cast<std::byte*>(buffer.pathBuffer) + buffer.printNameOffset);
    std::memcpy(printDestination, printName.data(), printBytes);
    printDestination[printName.size()] = L'\0';

    DWORD bytesReturned = 0U;
    const BOOL configured = DeviceIoControl(
        aliasHandle,
        FSCTL_SET_REPARSE_POINT,
        &buffer,
        static_cast<DWORD>(controlBufferLength),
        nullptr,
        0U,
        &bytesReturned,
        nullptr);
    const std::error_code reparseError = configured == FALSE
        ? std::error_code(static_cast<int>(GetLastError()), std::system_category())
        : std::error_code{};
    CloseHandle(aliasHandle);
    if (configured == FALSE) {
        (void)RemoveDirectoryW(alias.c_str());
        issue = "mount-point reparse configuration failed: " + reparseError.message();
        return false;
    }

    const DWORD attributes = GetFileAttributesW(alias.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES
        || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U) {
        const std::error_code attributesError(
            static_cast<int>(GetLastError()), std::system_category());
        (void)RemoveDirectoryW(alias.c_str());
        issue = "configured alias is not an inspectable reparse point: " + attributesError.message();
        return false;
    }
    return true;
#else
    std::error_code error;
    fs::create_directory_symlink(target, alias, error);
    if (error) {
        issue = "directory symlink creation failed: " + error.message();
        return false;
    }
    return true;
#endif
}

bool DetachDirectoryAlias(const fs::path& alias, std::string& issue) {
    issue.clear();
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(alias.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return true;
        }
        issue = "alias attributes cannot be inspected during cleanup: "
            + std::error_code(static_cast<int>(error), std::system_category()).message();
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U) {
        issue = "refusing cleanup because the expected alias is not a reparse point";
        return false;
    }
    if (RemoveDirectoryW(alias.c_str()) == FALSE) {
        issue = "alias detachment failed: "
            + std::error_code(static_cast<int>(GetLastError()), std::system_category()).message();
        return false;
    }
    return true;
#else
    std::error_code statusError;
    const fs::file_status status = fs::symlink_status(alias, statusError);
    if (statusError == std::errc::no_such_file_or_directory
        || status.type() == fs::file_type::not_found) {
        return true;
    }
    if (statusError) {
        issue = "alias cannot be inspected during cleanup: " + statusError.message();
        return false;
    }
    if (!fs::is_symlink(status)) {
        issue = "refusing cleanup because the expected alias is not a symlink";
        return false;
    }
    std::error_code removeError;
    if (!fs::remove(alias, removeError) || removeError) {
        issue = "alias detachment failed: " + removeError.message();
        return false;
    }
    return true;
#endif
}

bool CleanupFixtureTreeSafely(const fs::path& fixtureRoot, std::string& issue) {
    const fs::path aliases[] = {
        fixtureRoot / "unicode-project" / "assets" / "upper-link",
        fixtureRoot / "unicode-project" / "assets" / "lower-link",
    };
    for (const fs::path& alias : aliases) {
        if (!DetachDirectoryAlias(alias, issue)) {
            return false;
        }
    }
    std::error_code removeError;
    fs::remove_all(fixtureRoot, removeError);
    if (removeError) {
        issue = "fixture cleanup failed: " + removeError.message();
        return false;
    }
    return true;
}

bool CreatePackage(
    const fs::path& packageRoot,
    const std::string& packageId,
    const std::vector<std::string>& installPaths,
    ri::content::AssetPackageValidationReport& validation) {
    for (std::size_t index = 0U; index < installPaths.size(); ++index) {
        const fs::path documentPath = packageRoot / "assets"
            / ("payload-" + std::to_string(index) + ".ri_asset.json");
        std::error_code error;
        fs::create_directories(documentPath.parent_path(), error);
        if (error) {
            return false;
        }
        ri::content::AssetDocument document{};
        document.id = packageId + ".payload-" + std::to_string(index);
        document.type = "data";
        document.displayName = "Trust boundary payload " + std::to_string(index);
        document.sourcePath = "generated/payload-" + std::to_string(index) + ".json";
        document.payloadJson = R"({"trustBoundaryFixture":true})";
        if (!ri::content::SaveAssetDocument(documentPath, document)) {
            return false;
        }
    }

    ri::content::AssetPackageManifest manifest = ri::content::BuildAssetPackageManifest(
        packageRoot,
        packageId,
        packageId,
        "generated",
        "2026-08-03T00:00:00Z");
    manifest.packageVersion = "1.0.0";
    manifest.installScope = "project";
    if (manifest.assets.size() != installPaths.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < installPaths.size(); ++index) {
        manifest.assets[index].installPath = installPaths[index];
    }
    validation = ri::content::ValidateAssetPackageManifest(manifest, packageRoot);
    return ri::content::SaveAssetPackageManifest(
        packageRoot / "package.ri_package.json", manifest);
}

struct ProcessResult {
    bool launched = false;
    int exitCode = -1;
};

ProcessResult RunInstall(
    const fs::path& tool,
    const fs::path& packageRoot,
    const fs::path& projectRoot) {
#if defined(_WIN32)
    const std::wstring toolText = tool.native();
    const std::wstring packageText = packageRoot.native();
    const std::wstring projectText = projectRoot.native();
    const wchar_t* arguments[] = {
        toolText.c_str(),
        L"--asset-package-install",
        packageText.c_str(),
        L"--project",
        projectText.c_str(),
        nullptr,
    };
    const std::intptr_t result = _wspawnv(_P_WAIT, toolText.c_str(), arguments);
    return {
        .launched = result != -1,
        .exitCode = result == -1 ? -1 : static_cast<int>(result),
    };
#else
    const pid_t child = fork();
    if (child < 0) {
        return {};
    }
    if (child == 0) {
        execl(
            tool.c_str(),
            tool.c_str(),
            "--asset-package-install",
            packageRoot.c_str(),
            "--project",
            projectRoot.c_str(),
            static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status)) {
        return {};
    }
    const int exitCode = WEXITSTATUS(status);
    return {
        .launched = exitCode != 127,
        .exitCode = exitCode,
    };
#endif
}

} // namespace

int main(const int argc, const char* const* argv) {
    if (argc != 2) {
        return Fail("expected the ri_tool executable path");
    }
    const fs::path toolPath = fs::path(argv[1]);
    if (!fs::is_regular_file(toolPath)) {
        return Fail("ri_tool executable does not exist");
    }

#if defined(_WIN32)
    const auto processId = static_cast<std::uint64_t>(_getpid());
#else
    const auto processId = static_cast<std::uint64_t>(getpid());
#endif
    const auto uniqueTick = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path fixtureRoot = fs::temp_directory_path()
        / ("RawIronPackageInstallTrustBoundarySmoke."
           + std::to_string(processId) + "." + std::to_string(uniqueTick));
    std::error_code error;
    const bool createdFixtureRoot = fs::create_directory(fixtureRoot, error);
    if (error || !createdFixtureRoot) {
        return Fail("could not create integration fixture root");
    }
    const auto fail = [&](const std::string& message) {
        std::string cleanupIssue;
        if (!CleanupFixtureTreeSafely(fixtureRoot, cleanupIssue)) {
            return Fail(message + "; cleanup refused: " + cleanupIssue
                + "; fixture retained at " + fixtureRoot.string());
        }
        return Fail(message);
    };

    const fs::path unicodePackage = fixtureRoot / "unicode-package";
    const fs::path unicodeProject = fixtureRoot / "unicode-project";
    const fs::path collisionTarget = unicodeProject / "assets" / "collision-target";
    fs::create_directories(collisionTarget, error);
    if (error) {
        return fail("could not create Unicode collision project");
    }
    std::ofstream(unicodeProject / "sentinel.txt", std::ios::binary) << "project-before";

    const fs::path upperLink = unicodeProject / "assets" / "upper-link";
    const fs::path lowerLink = unicodeProject / "assets" / "lower-link";
    std::string aliasIssue;
    if (CreateDirectoryAlias(
            collisionTarget,
            fixtureRoot / "missing-parent" / "expected-alias-failure",
            aliasIssue)
        || aliasIssue.empty()) {
        return fail("directory alias helper did not report its expected failure");
    }
    if (!CreateDirectoryAlias(collisionTarget, upperLink, aliasIssue)) {
        return fail("could not create upper in-project alias: " + aliasIssue);
    }
    if (!CreateDirectoryAlias(collisionTarget, lowerLink, aliasIssue)) {
        return fail("could not create lower in-project alias: " + aliasIssue);
    }

    std::vector<std::string> collidingInstallPaths;
#if defined(_WIN32)
    std::string upperUnicodePath = "assets/upper-link/";
    upperUnicodePath += "\xc3\x84-collision.ri_asset.json";
    std::string lowerUnicodePath = "assets/lower-link/";
    lowerUnicodePath += "\xc3\xa4-collision.ri_asset.json";
    collidingInstallPaths = {upperUnicodePath, lowerUnicodePath};
#else
    collidingInstallPaths = {
        "assets/upper-link/Case-collision.ri_asset.json",
        "assets/lower-link/case-collision.ri_asset.json",
    };
#endif
    ri::content::AssetPackageValidationReport unicodeValidation{};
    if (!CreatePackage(
            unicodePackage,
            "rawiron.unicode-collision-e2e",
            collidingInstallPaths,
            unicodeValidation)) {
        return fail("could not create Unicode collision package");
    }
    if (!unicodeValidation.valid) {
        return fail("raw link-alias paths unexpectedly collided during manifest validation");
    }
    const ri::content::PackageInstallPathResolution upperResolution =
        ri::content::ResolvePackageInstallPath(unicodeProject, collidingInstallPaths[0]);
    const ri::content::PackageInstallPathResolution lowerResolution =
        ri::content::ResolvePackageInstallPath(unicodeProject, collidingInstallPaths[1]);
    if (!upperResolution.safe || !lowerResolution.safe
        || !ri::content::PackageInstallDestinationsCollide(
            upperResolution.destination, lowerResolution.destination)) {
        return fail("link aliases did not resolve to a platform-colliding destination pair");
    }

    const std::vector<std::string> unicodeProjectBefore = SnapshotTree(unicodeProject);
    const ProcessResult unicodeInstall = RunInstall(toolPath, unicodePackage, unicodeProject);
    if (!unicodeInstall.launched) {
        return fail("could not launch ri_tool for the collision regression");
    }
    if (unicodeInstall.exitCode == 0) {
        return fail("ri_tool accepted a package with colliding install destinations");
    }
    if (SnapshotTree(unicodeProject) != unicodeProjectBefore
        || ReadFile(unicodeProject / "sentinel.txt") != "project-before"
        || fs::exists(
            unicodeProject / "assets" / "package_receipts"
            / "rawiron.unicode-collision-e2e.ri_package.json")) {
        return fail("collision rejection mutated the project before aborting");
    }

    const fs::path hardLinkPackage = fixtureRoot / "hardlink-package";
    const fs::path hardLinkProject = fixtureRoot / "hardlink-project";
    fs::create_directories(hardLinkProject / "assets", error);
    if (error) {
        return fail("could not create hard-link project");
    }
    const fs::path outsideSentinel = fixtureRoot / "outside-hardlink-sentinel.txt";
    const fs::path linkedDestination =
        hardLinkProject / "assets" / "existing.ri_asset.json";
    std::ofstream(outsideSentinel, std::ios::binary) << "outside-before";
    error.clear();
    fs::create_hard_link(outsideSentinel, linkedDestination, error);
    if (error || fs::hard_link_count(outsideSentinel, error) < 2U) {
        return fail("could not create the hard-link integration fixture");
    }
    const ri::content::PackageInstallPathResolution hardLinkResolution =
        ri::content::ResolvePackageInstallPath(
            hardLinkProject, "assets/existing.ri_asset.json");
    if (hardLinkResolution.safe
        || hardLinkResolution.issue.find("multiple hard-link aliases") == std::string::npos) {
        return fail("hard-link destination was not rejected with the expected diagnostic");
    }

    ri::content::AssetPackageValidationReport hardLinkValidation{};
    if (!CreatePackage(
            hardLinkPackage,
            "rawiron.hardlink-e2e",
            {"assets/existing.ri_asset.json"},
            hardLinkValidation)
        || !hardLinkValidation.valid) {
        return fail("could not create a valid hard-link overwrite package");
    }
    const std::vector<std::string> hardLinkProjectBefore = SnapshotTree(hardLinkProject);
    const ProcessResult hardLinkInstall = RunInstall(toolPath, hardLinkPackage, hardLinkProject);
    if (!hardLinkInstall.launched) {
        return fail("could not launch ri_tool for the hard-link regression");
    }
    if (hardLinkInstall.exitCode == 0) {
        return fail("ri_tool accepted a multi-link overwrite destination");
    }
    if (SnapshotTree(hardLinkProject) != hardLinkProjectBefore
        || ReadFile(outsideSentinel) != "outside-before"
        || ReadFile(linkedDestination) != "outside-before"
        || fs::exists(
            hardLinkProject / "assets" / "package_receipts"
            / "rawiron.hardlink-e2e.ri_package.json")) {
        return fail("hard-link rejection mutated an outside alias or the project");
    }

    // Mid-promote failure: first destination is writable, second parent path is a file so
    // create_directories fails. Transactional install must roll back the first write.
    const fs::path rollbackPackage = fixtureRoot / "rollback-package";
    const fs::path rollbackProject = fixtureRoot / "rollback-project";
    fs::create_directories(rollbackProject / "assets", error);
    if (error) {
        return fail("could not create transactional rollback project");
    }
    std::ofstream(rollbackProject / "sentinel.txt", std::ios::binary) << "rollback-before";
    std::ofstream(rollbackProject / "assets" / "blocked", std::ios::binary) << "not-a-directory";
    std::ofstream(rollbackProject / "assets" / "keep-me.ri_asset.json", std::ios::binary)
        << "preexisting-keep";

    ri::content::AssetPackageValidationReport rollbackValidation{};
    if (!CreatePackage(
            rollbackPackage,
            "rawiron.rollback-e2e",
            {
                "assets/first.ri_asset.json",
                "assets/blocked/nested.ri_asset.json",
            },
            rollbackValidation)
        || !rollbackValidation.valid) {
        return fail("could not create transactional rollback package");
    }
    const std::vector<std::string> rollbackProjectBefore = SnapshotTree(rollbackProject);
    const ProcessResult rollbackInstall = RunInstall(toolPath, rollbackPackage, rollbackProject);
    if (!rollbackInstall.launched) {
        return fail("could not launch ri_tool for the transactional rollback regression");
    }
    if (rollbackInstall.exitCode == 0) {
        return fail("ri_tool accepted an install that cannot create a blocked destination parent");
    }
    if (SnapshotTree(rollbackProject) != rollbackProjectBefore
        || ReadFile(rollbackProject / "sentinel.txt") != "rollback-before"
        || ReadFile(rollbackProject / "assets" / "keep-me.ri_asset.json") != "preexisting-keep"
        || ReadFile(rollbackProject / "assets" / "blocked") != "not-a-directory"
        || fs::exists(rollbackProject / "assets" / "first.ri_asset.json")
        || fs::exists(
            rollbackProject / "assets" / "package_receipts"
            / "rawiron.rollback-e2e.ri_package.json")) {
        return fail("transactional install did not roll back after a mid-promote failure");
    }

#if defined(_WIN32)
    // Overwrite must record the promotion before mutating so a failed replace
    // still restores the pre-install bytes (readonly destination forces the write to fail).
    const fs::path readonlyPackage = fixtureRoot / "readonly-package";
    const fs::path readonlyProject = fixtureRoot / "readonly-project";
    fs::create_directories(readonlyProject / "assets", error);
    if (error) {
        return fail("could not create readonly overwrite project");
    }
    const fs::path readonlyDestination = readonlyProject / "assets" / "overwrite-me.ri_asset.json";
    std::ofstream(readonlyDestination, std::ios::binary) << "readonly-original";
    if (SetFileAttributesW(readonlyDestination.c_str(), FILE_ATTRIBUTE_READONLY) == FALSE) {
        return fail("could not mark the overwrite destination read-only");
    }
    ri::content::AssetPackageValidationReport readonlyValidation{};
    if (!CreatePackage(
            readonlyPackage,
            "rawiron.readonly-e2e",
            {"assets/overwrite-me.ri_asset.json"},
            readonlyValidation)
        || !readonlyValidation.valid) {
        SetFileAttributesW(readonlyDestination.c_str(), FILE_ATTRIBUTE_NORMAL);
        return fail("could not create readonly overwrite package");
    }
    const ProcessResult readonlyInstall = RunInstall(toolPath, readonlyPackage, readonlyProject);
    const DWORD destinationAttributes = GetFileAttributesW(readonlyDestination.c_str());
    SetFileAttributesW(readonlyDestination.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (!readonlyInstall.launched) {
        return fail("could not launch ri_tool for the readonly overwrite regression");
    }
    if (readonlyInstall.exitCode == 0
        || ReadFile(readonlyDestination) != "readonly-original"
        || (destinationAttributes != INVALID_FILE_ATTRIBUTES
            && (destinationAttributes & FILE_ATTRIBUTE_READONLY) == 0U)
        || fs::exists(
            readonlyProject / "assets" / "package_receipts"
            / "rawiron.readonly-e2e.ri_package.json")) {
        return fail("failed overwrite did not preserve the original destination");
    }
#endif

    // Happy path: multi-file install with overwrite + receipt, proving stage/promote succeeds.
    const fs::path happyPackage = fixtureRoot / "happy-package";
    const fs::path happyProject = fixtureRoot / "happy-project";
    fs::create_directories(happyProject / "assets", error);
    if (error) {
        return fail("could not create happy-path install project");
    }
    std::ofstream(happyProject / "assets" / "overwrite-me.ri_asset.json", std::ios::binary)
        << "old-payload";

    ri::content::AssetPackageValidationReport happyValidation{};
    if (!CreatePackage(
            happyPackage,
            "rawiron.happy-e2e",
            {
                "assets/overwrite-me.ri_asset.json",
                "assets/packages/rawiron.happy-e2e/new-payload.ri_asset.json",
            },
            happyValidation)
        || !happyValidation.valid) {
        return fail("could not create happy-path install package");
    }
    const ProcessResult happyInstall = RunInstall(toolPath, happyPackage, happyProject);
    if (!happyInstall.launched || happyInstall.exitCode != 0) {
        return fail("happy-path transactional install failed");
    }
    if (!fs::exists(happyProject / "assets" / "overwrite-me.ri_asset.json")
        || !fs::exists(
            happyProject / "assets" / "packages" / "rawiron.happy-e2e"
            / "new-payload.ri_asset.json")
        || !fs::exists(
            happyProject / "assets" / "package_receipts"
            / "rawiron.happy-e2e.ri_package.json")
        || ReadFile(happyProject / "assets" / "overwrite-me.ri_asset.json") == "old-payload") {
        return fail("happy-path transactional install did not publish expected project files");
    }

    std::string cleanupIssue;
    if (!CleanupFixtureTreeSafely(fixtureRoot, cleanupIssue)) {
        return Fail("successful fixture cleanup refused: " + cleanupIssue
            + "; fixture retained at " + fixtureRoot.string());
    }
    return EXIT_SUCCESS;
}
