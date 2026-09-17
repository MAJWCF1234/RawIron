#version 450
#extension GL_GOOGLE_include_directive : require
layout(location = 0) out vec4 fragColor;
#include "SerenityColorCommon.glsl"
#include "SerenityExposure.glsl"
void main() {
    fragColor = SerenityMeterExposureRod();
}
