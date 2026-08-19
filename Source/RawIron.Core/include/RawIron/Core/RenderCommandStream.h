#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace ri::core {

[[nodiscard]] constexpr std::uint64_t PackRenderSortKey(std::uint8_t passIndex,
                                                        std::uint16_t pipelineBucket,
                                                        std::uint16_t materialBucket,
                                                        std::uint16_t depthBucket,
                                                        std::uint8_t tieBreaker = 0) noexcept {
    return (static_cast<std::uint64_t>(passIndex) << 56U)
        | (static_cast<std::uint64_t>(pipelineBucket) << 40U)
        | (static_cast<std::uint64_t>(materialBucket) << 24U)
        | (static_cast<std::uint64_t>(depthBucket) << 8U)
        | static_cast<std::uint64_t>(tieBreaker);
}

enum class RenderCommandType : std::uint16_t {
    ClearColor = 1,
    SetViewProjection = 2,
    DrawMesh = 3,
};

struct RenderCommandHeader {
    RenderCommandType type = RenderCommandType::ClearColor;
    std::uint16_t sizeBytes = 0;
    std::uint64_t sortKey = 0;
    std::uint32_t sequence = 0;
};

struct ClearColorCommand {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct SetViewProjectionCommand {
    float viewProjection[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
};

struct DrawMeshCommand {
    std::int32_t meshHandle = -1;
    std::int32_t materialHandle = -1;
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t instanceCount = 1;
    float model[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
};

struct RenderCommandView;

class RenderCommandStream {
public:
    void Clear() noexcept;
    void Reserve(std::size_t bytes);
    [[nodiscard]] std::size_t SizeBytes() const noexcept;
    [[nodiscard]] std::size_t CommandCount() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> Bytes() const noexcept;
    [[nodiscard]] std::vector<std::size_t> BuildSortedPacketOrder() const;
    [[nodiscard]] bool ReadPacket(std::size_t packetIndex, RenderCommandView& outView) const noexcept;

    template <typename TPayload>
    void Emit(RenderCommandType type, const TPayload& payload) {
        EmitSorted(type, payload, 0);
    }

    template <typename TPayload>
    void EmitSorted(RenderCommandType type, const TPayload& payload, std::uint64_t sortKey) {
        static_assert(std::is_trivially_copyable_v<TPayload>,
                      "Render command payloads must be trivially copyable.");
        static_assert(sizeof(TPayload) <= 0xFFFFu,
                      "Render command payload is too large for 16-bit packet size.");

        constexpr std::size_t packetSize = sizeof(RenderCommandHeader) + sizeof(TPayload);
        const std::size_t previousSize = bytes_.size();
        if (packetSize > bytes_.max_size() || previousSize > bytes_.max_size() - packetSize) {
            throw std::length_error("Render command stream exceeds addressable storage.");
        }
        if (packetOffsets_.size() == packetOffsets_.max_size()) {
            throw std::length_error("Render command stream contains too many packets.");
        }

        // Make the only potentially allocating metadata operation happen before bytes_ changes.
        // This keeps the stream logically unchanged if allocation fails.
        if (packetOffsets_.size() == packetOffsets_.capacity()) {
            const std::size_t currentCapacity = packetOffsets_.capacity();
            const std::size_t maxCapacity = packetOffsets_.max_size();
            const std::size_t grownCapacity = currentCapacity > (maxCapacity / 2U)
                ? maxCapacity
                : std::max<std::size_t>(1U, currentCapacity * 2U);
            packetOffsets_.reserve(grownCapacity);
        }

        bytes_.resize(previousSize + packetSize);
        const RenderCommandHeader header{
            .type = type,
            .sizeBytes = static_cast<std::uint16_t>(sizeof(TPayload)),
            .sortKey = sortKey,
            .sequence = nextSequence_++,
        };
        std::memcpy(bytes_.data() + previousSize, &header, sizeof(RenderCommandHeader));
        std::memcpy(bytes_.data() + previousSize + sizeof(RenderCommandHeader), &payload, sizeof(TPayload));
        packetOffsets_.push_back(previousSize);
        ++commandCount_;
    }

private:
    std::vector<std::uint8_t> bytes_{};
    std::vector<std::size_t> packetOffsets_{};
    std::size_t commandCount_ = 0;
    std::uint32_t nextSequence_ = 0;
};

struct RenderCommandView {
    RenderCommandHeader header{};
    const std::uint8_t* payload = nullptr;

    template <typename TPayload>
    [[nodiscard]] bool ReadPayload(TPayload& outPayload) const noexcept {
        static_assert(std::is_trivially_copyable_v<TPayload>,
                      "Render command payloads must be trivially copyable.");
        if (payload == nullptr || header.sizeBytes != sizeof(TPayload)) {
            return false;
        }
        std::memcpy(&outPayload, payload, sizeof(TPayload));
        return true;
    }
};

class RenderCommandReader {
public:
    explicit RenderCommandReader(std::span<const std::uint8_t> bytes) noexcept;

    [[nodiscard]] bool Next(RenderCommandView& outView) noexcept;
    [[nodiscard]] bool Exhausted() const noexcept;

private:
    std::span<const std::uint8_t> bytes_{};
    std::size_t cursor_ = 0;
};

} // namespace ri::core
