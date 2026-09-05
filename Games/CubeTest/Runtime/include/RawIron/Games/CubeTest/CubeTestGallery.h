#pragma once

#include <span>
#include <string>
#include <string_view>
#include "RawIron/Math/Vec3.h"

namespace ri::games::cubetest {

struct GalleryRoomGuide {
    std::string_view id;
    std::string_view title;
    float centerX;
    std::string_view subsystem;
    std::string_view reference;
    std::string_view controls;
    std::string_view observation;
};

[[nodiscard]] std::span<const GalleryRoomGuide> CubeTestRoomGuides();
[[nodiscard]] const GalleryRoomGuide* FindCubeTestRoom(std::string_view id);
[[nodiscard]] ri::math::Vec3 CubeTestRoomArrival(const GalleryRoomGuide& room);
[[nodiscard]] const GalleryRoomGuide& CubeTestRoomAt(float worldX);
[[nodiscard]] std::string DescribeCubeTestRoom(const GalleryRoomGuide& room);
[[nodiscard]] std::string CubeTestGalleryHelp();

} // namespace ri::games::cubetest
