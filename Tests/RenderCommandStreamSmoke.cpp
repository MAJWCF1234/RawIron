#include "RawIron/Core/RenderCommandStream.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

int main() {
    ri::core::RenderCommandStream stream;
    stream.EmitSorted(
        ri::core::RenderCommandType::ClearColor,
        ri::core::ClearColorCommand{.r = 0.25f, .g = 0.5f, .b = 0.75f, .a = 1.0f},
        42U);

    ri::core::RenderCommandReader validReader(stream.Bytes());
    ri::core::RenderCommandView validView{};
    ri::core::ClearColorCommand clear{};
    if (!validReader.Next(validView)
        || !validView.ReadPayload(clear)
        || clear.r != 0.25f
        || clear.g != 0.5f
        || clear.b != 0.75f
        || clear.a != 1.0f
        || !validReader.Exhausted()) {
        return EXIT_FAILURE;
    }
    ri::core::DrawMeshCommand wrongPayload{};
    if (validView.ReadPayload(wrongPayload)) {
        return EXIT_FAILURE;
    }

    // Failed indexed reads and failed sequential reads must clear stale pointers. Otherwise a
    // caller that checks the view after an error can accidentally reuse the previous command.
    if (stream.ReadPacket(std::numeric_limits<std::size_t>::max(), validView)
        || validView.payload != nullptr
        || validView.header.sizeBytes != 0U) {
        return EXIT_FAILURE;
    }
    ri::core::RenderCommandReader emptyReader({});
    validView.payload = reinterpret_cast<const std::uint8_t*>(1U);
    validView.header.sizeBytes = 42U;
    if (emptyReader.Next(validView) || validView.payload != nullptr || validView.header.sizeBytes != 0U
        || !emptyReader.Exhausted()) {
        return EXIT_FAILURE;
    }

    // A stream ending in a partial header must fail closed and become exhausted. Before the fix,
    // `Next` returned false but left the cursor on the same bytes, which let consumer loops retry
    // forever when they were driven by `Exhausted`.
    std::vector<std::uint8_t> partialHeader(sizeof(ri::core::RenderCommandHeader) - 1U, 0U);
    ri::core::RenderCommandReader partialHeaderReader(partialHeader);
    ri::core::RenderCommandView malformedView{};
    malformedView.payload = reinterpret_cast<const std::uint8_t*>(1U);
    malformedView.header.sizeBytes = 42U;
    if (partialHeaderReader.Next(malformedView) || !partialHeaderReader.Exhausted()) {
        return EXIT_FAILURE;
    }
    if (malformedView.payload != nullptr || malformedView.header.sizeBytes != 0U) {
        return EXIT_FAILURE;
    }

    // The existing malformed-payload path must have the same terminal behavior.
    std::vector<std::uint8_t> partialPayload(sizeof(ri::core::RenderCommandHeader), 0U);
    ri::core::RenderCommandHeader malformedHeader{};
    malformedHeader.type = ri::core::RenderCommandType::DrawMesh;
    malformedHeader.sizeBytes = static_cast<std::uint16_t>(sizeof(ri::core::DrawMeshCommand));
    std::memcpy(partialPayload.data(), &malformedHeader, sizeof(malformedHeader));
    ri::core::RenderCommandReader partialPayloadReader(partialPayload);
    if (partialPayloadReader.Next(malformedView) || !partialPayloadReader.Exhausted()) {
        return EXIT_FAILURE;
    }
    if (malformedView.payload != nullptr || malformedView.header.sizeBytes != 0U) {
        return EXIT_FAILURE;
    }

    // Equal-key packets retain emission order independently of the compact diagnostic sequence.
    stream.EmitSorted(ri::core::RenderCommandType::ClearColor,
                      ri::core::ClearColorCommand{.r = 1.0f}, 7U);
    stream.EmitSorted(ri::core::RenderCommandType::ClearColor,
                      ri::core::ClearColorCommand{.r = 2.0f}, 7U);
    stream.EmitSorted(ri::core::RenderCommandType::ClearColor,
                      ri::core::ClearColorCommand{.r = 3.0f}, 7U);
    const std::vector<std::size_t> stableOrder = stream.BuildSortedPacketOrder();
    if (stableOrder != std::vector<std::size_t>{1U, 2U, 3U, 0U}) {
        return EXIT_FAILURE;
    }

    stream.Clear();
    if (stream.CommandCount() != 0U || stream.SizeBytes() != 0U || !stream.Bytes().empty()
        || !stream.BuildSortedPacketOrder().empty()) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
