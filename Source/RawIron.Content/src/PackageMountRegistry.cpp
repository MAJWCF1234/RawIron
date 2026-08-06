#include "RawIron/Content/PackageMountRegistry.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <limits.h>
#endif
#endif

namespace ri::content {
namespace {

namespace fs = std::filesystem;

[[nodiscard]] bool IsSafeRelativePath(const std::string_view rawPath) {
    if (rawPath.empty()) {
        return false;
    }
    const fs::path path(rawPath);
    if (path.is_absolute()) {
        return false;
    }
    for (const fs::path& component : path) {
        if (component.empty() || component == "." || component == "..") {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsPathWithin(const fs::path& root, const fs::path& candidate) {
    auto rootIt = root.begin();
    auto candidateIt = candidate.begin();
    for (; rootIt != root.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == candidate.end()) {
            return false;
        }
#if defined(_WIN32)
        // Match AssetPackageManifest: Windows paths are case-insensitive.
        if (CompareStringOrdinal(rootIt->c_str(), -1, candidateIt->c_str(), -1, TRUE) != CSTR_EQUAL) {
            return false;
        }
#else
        if (*rootIt != *candidateIt) {
            return false;
        }
#endif
    }
    return true;
}

[[nodiscard]] std::string NormalizeVirtualPath(const fs::path& path) {
    std::string normalized = path.lexically_normal().generic_string();
    while (normalized.starts_with("./")) {
        normalized.erase(0U, 2U);
    }
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

[[nodiscard]] std::string EffectiveMountPoint(const AssetPackageManifest& manifest) {
    if (!manifest.mountPoint.empty()) {
        return NormalizeVirtualPath(fs::path(manifest.mountPoint));
    }
    return "Packages/" + manifest.packageId;
}

[[nodiscard]] std::optional<fs::path> ResolveContainedFile(
    const fs::path& packageRoot,
    const std::string_view relativePath) {
    if (!IsSafeRelativePath(relativePath)) {
        return std::nullopt;
    }
    std::error_code rootError;
    std::error_code fileError;
    const fs::path canonicalRoot = fs::weakly_canonical(packageRoot, rootError);
    const fs::path canonicalFile =
        fs::weakly_canonical(packageRoot / fs::path(relativePath), fileError);
    if (rootError || fileError || !IsPathWithin(canonicalRoot, canonicalFile)
        || !fs::is_regular_file(canonicalFile)) {
        return std::nullopt;
    }
    return canonicalFile;
}

/// Re-check a cached absolute path still resolves under the package root.
/// Open reparse-safe first so a symlink swap between attribute probes and
/// weakly_canonical cannot redirect containment to a different target.
[[nodiscard]] std::optional<fs::path> ResolveStillContained(
    const fs::path& packageRoot,
    const fs::path& cachedPhysicalPath) {
    std::error_code rootError;
    const fs::path canonicalRoot = fs::weakly_canonical(packageRoot, rootError);
    if (rootError || canonicalRoot.empty()) {
        return std::nullopt;
    }
#if defined(_WIN32)
    const HANDLE fileHandle = CreateFileW(cachedPhysicalPath.c_str(),
                                          GENERIC_READ,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                          nullptr,
                                          OPEN_EXISTING,
                                          FILE_FLAG_OPEN_REPARSE_POINT,
                                          nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(fileHandle, &info)
        || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U
        || (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U
        || info.nNumberOfLinks > 1U) {
        CloseHandle(fileHandle);
        return std::nullopt;
    }
    std::wstring finalPath;
    constexpr DWORD kMaxFinalPathChars = 32768U;
    for (DWORD capacity = MAX_PATH;;) {
        finalPath.assign(capacity, L'\0');
        const DWORD length =
            GetFinalPathNameByHandleW(fileHandle, finalPath.data(), capacity, FILE_NAME_NORMALIZED);
        if (length == 0U) {
            CloseHandle(fileHandle);
            return std::nullopt;
        }
        if (length < capacity) {
            finalPath.resize(length);
            break;
        }
        if (capacity >= kMaxFinalPathChars) {
            CloseHandle(fileHandle);
            return std::nullopt;
        }
        const DWORD nextCapacity = capacity * 2U;
        capacity = nextCapacity < capacity || nextCapacity > kMaxFinalPathChars
            ? kMaxFinalPathChars
            : nextCapacity;
    }
    CloseHandle(fileHandle);
    fs::path canonicalFile(finalPath);
    // Strip the \\?\ / \\?\UNC\ prefix so IsPathWithin matches weakly_canonical roots.
    const std::wstring& wide = canonicalFile.native();
    if (wide.rfind(L"\\\\?\\UNC\\", 0) == 0) {
        canonicalFile = fs::path(L"\\\\" + wide.substr(8));
    } else if (wide.rfind(L"\\\\?\\", 0) == 0) {
        canonicalFile = fs::path(wide.substr(4));
    }
    canonicalFile = canonicalFile.lexically_normal();
    if (!IsPathWithin(canonicalRoot, canonicalFile)) {
        return std::nullopt;
    }
    return canonicalFile;
#else
    int flags = O_RDONLY;
#if defined(O_CLOEXEC)
    flags |= O_CLOEXEC;
#endif
#if defined(O_NOFOLLOW)
    flags |= O_NOFOLLOW;
#endif
    const int fd = ::open(cachedPhysicalPath.c_str(), flags);
    if (fd < 0) {
        return std::nullopt;
    }
    struct stat status {};
    if (::fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) || status.st_nlink > 1) {
        ::close(fd);
        return std::nullopt;
    }
    // Resolve the path of the open fd before closing so a symlink swap cannot
    // redirect weakly_canonical to a different target.
    fs::path canonicalFile;
#if defined(__linux__)
    char fdPath[64];
    std::snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", fd);
    char resolved[4096]{};
    const ssize_t resolvedLength = ::readlink(fdPath, resolved, sizeof(resolved) - 1U);
    ::close(fd);
    if (resolvedLength <= 0) {
        return std::nullopt;
    }
    resolved[resolvedLength] = '\0';
    canonicalFile = fs::path(resolved).lexically_normal();
#elif defined(__APPLE__)
    char resolved[PATH_MAX];
    if (::fcntl(fd, F_GETPATH, resolved) == -1) {
        ::close(fd);
        return std::nullopt;
    }
    ::close(fd);
    canonicalFile = fs::path(resolved).lexically_normal();
#else
    ::close(fd);
    return std::nullopt;
#endif
    if (!IsPathWithin(canonicalRoot, canonicalFile)) {
        return std::nullopt;
    }
    return canonicalFile;
#endif
}

} // namespace

PackageActivationResult PackageMountRegistry::Activate(
    const std::span<const InstalledAssetPackage> catalog,
    const std::span<const PackageRequest> roots,
    const PackageResolverOptions& options) {
    PackageActivationResult result{};

    std::vector<bool> excluded(catalog.size(), false);
    std::vector<std::optional<AssetPackageValidationReport>> validationCache(catalog.size());
    std::map<std::string, std::vector<std::string>, std::less<>> invalidIssues;
    PackageResolutionResult resolution{};
    std::vector<std::size_t> resolvedInstalledIndices;
    for (;;) {
        std::vector<AssetPackageManifest> candidateManifests;
        std::vector<std::size_t> candidateToInstalled;
        candidateManifests.reserve(catalog.size());
        candidateToInstalled.reserve(catalog.size());
        for (std::size_t index = 0; index < catalog.size(); ++index) {
            if (!excluded[index]) {
                candidateManifests.push_back(catalog[index].manifest);
                candidateToInstalled.push_back(index);
            }
        }

        resolution = ResolvePackages(candidateManifests, roots, options);
        if (!resolution.resolved) {
            result.issues = resolution.issues;
            for (const auto& [id, issues] : invalidIssues) {
                result.issues.push_back("Package '" + id + "' failed validation:");
                for (const std::string& issue : issues) {
                    result.issues.push_back("  " + issue);
                }
            }
            return result;
        }

        bool rejectedCandidate = false;
        resolvedInstalledIndices.clear();
        for (const ResolvedPackage& resolved : resolution.loadOrder) {
            const std::size_t installedIndex = candidateToInstalled.at(resolved.catalogIndex);
            if (!validationCache[installedIndex].has_value()) {
                validationCache[installedIndex] = ValidateAssetPackageManifest(
                    catalog[installedIndex].manifest,
                    catalog[installedIndex].packageRoot);
            }
            const AssetPackageValidationReport& validation = *validationCache[installedIndex];
            if (!validation.valid) {
                excluded[installedIndex] = true;
                invalidIssues[catalog[installedIndex].manifest.packageId] = validation.issues;
                rejectedCandidate = true;
            } else {
                resolvedInstalledIndices.push_back(installedIndex);
            }
        }
        if (!rejectedCandidate) {
            break;
        }
    }

    struct Candidate {
        const InstalledAssetPackage* package = nullptr;
        const AssetPackageValidationReport* validation = nullptr;
        std::string mountPoint{};
        std::vector<std::pair<std::string, fs::path>> assets{};
        std::optional<fs::path> runtimeEntry{};
    };
    std::vector<Candidate> candidates;
    candidates.reserve(resolvedInstalledIndices.size());
    for (const std::size_t installedIndex : resolvedInstalledIndices) {
        const InstalledAssetPackage& package = catalog[installedIndex];
        candidates.push_back({
            &package,
            &*validationCache[installedIndex],
            EffectiveMountPoint(package.manifest),
        });
    }

    std::scoped_lock lock(mutex_);
    std::set<std::string, std::less<>> requestedMountPoints;
    for (const Candidate& candidate : candidates) {
        const AssetPackageManifest& manifest = candidate.package->manifest;
        const auto existing = mounted_.find(manifest.packageId);
        if (existing != mounted_.end()) {
            std::error_code existingError;
            std::error_code candidateError;
            const fs::path existingRoot =
                fs::weakly_canonical(existing->second.package.packageRoot, existingError);
            const fs::path candidateRoot =
                fs::weakly_canonical(candidate.package->packageRoot, candidateError);
            if (existing->second.package.manifest.packageVersion != manifest.packageVersion
                || existingError || candidateError || existingRoot != candidateRoot) {
                result.issues.push_back(
                    "Package '" + manifest.packageId + "' is already mounted from a different version or root.");
                return result;
            }
            if (existing->second.referenceCount == std::numeric_limits<std::size_t>::max()) {
                result.issues.push_back(
                    "Package '" + manifest.packageId + "' reference count is exhausted.");
                return result;
            }
            continue;
        }
        if (!requestedMountPoints.insert(candidate.mountPoint).second) {
            result.issues.push_back(
                "Multiple packages in the activation graph request mount point '" + candidate.mountPoint + "'.");
            return result;
        }
        const auto collision = std::find_if(mounted_.begin(), mounted_.end(), [&](const auto& active) {
            return active.second.mountPoint == candidate.mountPoint
                && active.first != manifest.packageId;
        });
        if (collision != mounted_.end()) {
            result.issues.push_back(
                "Mount point '" + candidate.mountPoint + "' is already owned by package '"
                + collision->first + "'.");
            return result;
        }
    }

    for (Candidate& candidate : candidates) {
        if (mounted_.contains(candidate.package->manifest.packageId)) {
            continue;
        }
        candidate.assets.reserve(candidate.package->manifest.assets.size());
        for (const AssetPackageEntry& asset : candidate.package->manifest.assets) {
            const std::optional<fs::path> physical =
                ResolveContainedFile(candidate.package->packageRoot, asset.path);
            if (!physical.has_value()) {
                result.issues.push_back(
                    "Package '" + candidate.package->manifest.packageId
                    + "' changed after validation; asset '" + asset.id + "' is unavailable.");
                return result;
            }
            candidate.assets.emplace_back(asset.id, *physical);
        }
        if (candidate.package->manifest.runtime.executionMode != "data") {
            candidate.runtimeEntry = ResolveContainedFile(
                candidate.package->packageRoot,
                candidate.package->manifest.runtime.entryPoint);
            if (!candidate.runtimeEntry.has_value()) {
                result.issues.push_back(
                    "Package '" + candidate.package->manifest.packageId
                    + "' changed after validation; its runtime entry point is unavailable.");
                return result;
            }
        }
    }

    std::uint64_t activationId = nextActivationId_++;
    while (activationId == 0U || activations_.contains(activationId)) {
        activationId = nextActivationId_++;
    }
    std::vector<std::string> activationPackages;
    activationPackages.reserve(candidates.size());
    for (const Candidate& candidate : candidates) {
        const AssetPackageManifest& manifest = candidate.package->manifest;
        const auto existing = mounted_.find(manifest.packageId);
        if (existing != mounted_.end()) {
            ++existing->second.referenceCount;
            result.retained.push_back(manifest.packageId);
        } else {
            MountedRecord record{};
            record.package = *candidate.package;
            record.package.validation = *candidate.validation;
            record.mountPoint = candidate.mountPoint;
            record.referenceCount = 1U;
            record.mountOrdinal = nextMountOrdinal_++;
            record.runtimeEntry = candidate.runtimeEntry;
            for (const auto& [assetId, physicalPath] : candidate.assets) {
                record.assetsById.emplace(assetId, physicalPath);
            }
            for (std::size_t index = 0U; index < manifest.assets.size(); ++index) {
                const AssetPackageEntry& asset = manifest.assets[index];
                virtualAssets_.emplace(
                    NormalizeVirtualPath(fs::path(record.mountPoint) / fs::path(asset.path)),
                    VirtualAssetRecord{
                        .packageId = manifest.packageId,
                        .physicalPath = candidate.assets[index].second,
                    });
            }
            mounted_.emplace(manifest.packageId, std::move(record));
            result.newlyMounted.push_back(manifest.packageId);
        }
        activationPackages.push_back(manifest.packageId);
    }
    activations_.emplace(activationId, std::move(activationPackages));
    result.activated = true;
    result.activationId = activationId;
    return result;
}

bool PackageMountRegistry::Deactivate(const std::uint64_t activationId) {
    std::scoped_lock lock(mutex_);
    const auto activation = activations_.find(activationId);
    if (activation == activations_.end()) {
        return false;
    }
    for (auto packageIt = activation->second.rbegin(); packageIt != activation->second.rend(); ++packageIt) {
        const auto mounted = mounted_.find(*packageIt);
        if (mounted == mounted_.end()) {
            continue;
        }
        if (mounted->second.referenceCount > 1U) {
            --mounted->second.referenceCount;
        } else {
            for (const AssetPackageEntry& asset : mounted->second.package.manifest.assets) {
                virtualAssets_.erase(NormalizeVirtualPath(
                    fs::path(mounted->second.mountPoint) / fs::path(asset.path)));
            }
            mounted_.erase(mounted);
        }
    }
    activations_.erase(activation);
    return true;
}

void PackageMountRegistry::DeactivateAll() {
    std::scoped_lock lock(mutex_);
    activations_.clear();
    mounted_.clear();
    virtualAssets_.clear();
}

bool PackageMountRegistry::IsMounted(const std::string_view packageId) const {
    std::scoped_lock lock(mutex_);
    return mounted_.contains(std::string(packageId));
}

std::size_t PackageMountRegistry::ActiveActivationCount() const {
    std::scoped_lock lock(mutex_);
    return activations_.size();
}

std::vector<MountedPackageInfo> PackageMountRegistry::MountedPackages() const {
    std::scoped_lock lock(mutex_);
    std::vector<std::pair<std::uint64_t, MountedPackageInfo>> ordered;
    ordered.reserve(mounted_.size());
    for (const auto& [id, record] : mounted_) {
        ordered.push_back({
            record.mountOrdinal,
            {
                .packageId = id,
                .packageVersion = record.package.manifest.packageVersion,
                .packageKind = record.package.manifest.packageKind,
                .mountPoint = record.mountPoint,
                .packageRoot = record.package.packageRoot,
                .referenceCount = record.referenceCount,
            },
        });
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });
    std::vector<MountedPackageInfo> result;
    result.reserve(ordered.size());
    for (auto& [ordinal, info] : ordered) {
        (void)ordinal;
        result.push_back(std::move(info));
    }
    return result;
}

std::optional<fs::path> PackageMountRegistry::ResolveAsset(
    const std::string_view packageId,
    const std::string_view assetId) const {
    std::scoped_lock lock(mutex_);
    const auto mounted = mounted_.find(std::string(packageId));
    if (mounted == mounted_.end()) {
        return std::nullopt;
    }
    const auto asset = mounted->second.assetsById.find(std::string(assetId));
    if (asset == mounted->second.assetsById.end()) {
        return std::nullopt;
    }
    // Re-validate containment on every resolve so a post-mount symlink swap cannot
    // escape the package root through a stale cached absolute path.
    return ResolveStillContained(mounted->second.package.packageRoot, asset->second);
}

std::optional<fs::path> PackageMountRegistry::ResolveVirtualPath(
    const std::string_view virtualPath) const {
    if (!IsSafeRelativePath(virtualPath)) {
        return std::nullopt;
    }
    const std::string normalized = NormalizeVirtualPath(fs::path(virtualPath));
    std::scoped_lock lock(mutex_);
    const auto virtualAsset = virtualAssets_.find(normalized);
    if (virtualAsset == virtualAssets_.end()) {
        return std::nullopt;
    }
    const auto mounted = mounted_.find(virtualAsset->second.packageId);
    if (mounted == mounted_.end()) {
        return std::nullopt;
    }
    return ResolveStillContained(mounted->second.package.packageRoot, virtualAsset->second.physicalPath);
}

std::optional<fs::path> PackageMountRegistry::ResolveRuntimeEntry(
    const std::string_view packageId) const {
    std::scoped_lock lock(mutex_);
    const auto mounted = mounted_.find(std::string(packageId));
    if (mounted == mounted_.end() || !mounted->second.runtimeEntry.has_value()) {
        return std::nullopt;
    }
    return ResolveStillContained(mounted->second.package.packageRoot, *mounted->second.runtimeEntry);
}

} // namespace ri::content
