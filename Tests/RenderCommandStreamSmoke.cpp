#include "RawIron/Core/RenderCommandStream.h"

#include <cstdlib>
#include <cstring>
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

    // A stream ending in a partial header must fail closed and become exhausted. Before the fix,
    // `Next` returned false but left the cursor on the same bytes, which let consumer loops retry
    // forever when they were driven by `Exhausted`.
    std::vector<std::uint8_t> partialHeader(sizeof(ri::core::RenderCommandHeader) - 1U, 0U);
    ri::core::RenderCommandReader partialHeaderReader(partialHeader);
    ri::core::RenderCommandView malformedView{};
    if (partialHeaderReader.Next(malformedView) || !partialHeaderReader.Exhausted()) {
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

    return EXIT_SUCCESS;
}
