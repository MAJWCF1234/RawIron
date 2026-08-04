#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace ri::tooling {

/// Resource ceilings applied before and during extraction of ZIP-compatible
/// RawIron packages. These are security limits, not estimates.
struct SecureRipakLimits {
    std::uint64_t maximumArchiveBytes = 512ULL * 1024ULL * 1024ULL;
    std::size_t maximumEntries = 16384U;
    std::uint64_t maximumFileBytes = 256ULL * 1024ULL * 1024ULL;
    std::uint64_t maximumExpandedBytes = 1024ULL * 1024ULL * 1024ULL;
    std::uint64_t maximumCompressionRatio = 200ULL;
};

/// Owns an exclusive temporary extraction directory. Destruction removes the
/// directory recursively; moving transfers that cleanup responsibility.
class SecureRipakExtraction final {
public:
    SecureRipakExtraction() = default;
    ~SecureRipakExtraction() noexcept;

    SecureRipakExtraction(const SecureRipakExtraction&) = delete;
    SecureRipakExtraction& operator=(const SecureRipakExtraction&) = delete;
    SecureRipakExtraction(SecureRipakExtraction&& other) noexcept;
    SecureRipakExtraction& operator=(SecureRipakExtraction&& other) noexcept;

    [[nodiscard]] static SecureRipakExtraction Extract(
        const std::filesystem::path& archivePath,
        const SecureRipakLimits& limits = {});

    [[nodiscard]] const std::filesystem::path& Root() const noexcept { return root_; }
    [[nodiscard]] explicit operator bool() const noexcept { return !root_.empty(); }

private:
    explicit SecureRipakExtraction(std::filesystem::path root) : root_(std::move(root)) {}
    void Cleanup() noexcept;

    std::filesystem::path root_{};
};

} // namespace ri::tooling
