#version 450
#extension GL_GOOGLE_include_directive : require
layout(location = 0) out vec2 vUv;
layout(location = 1) flat out float vExposure;
layout(location = 2) flat out float vRodExposure;
#include "SerenityColorCommon.glsl"
#include "SerenityExposure.glsl"

void main() {
    const vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);
    vUv = pos * 0.5 + 0.5;
    vec4 meter = texelFetch(serenityExposureHistory, ivec2(0), 0);
    bool automatic = (int(serenity.controls[6].w + 0.5) & 1) != 0;
    vExposure = automatic ? meter.z * serenity.controls[0].x : serenity.exposureTuning.x;
    vRodExposure = automatic ? meter.w : clamp(log(serenity.exposureTuning.x * 2.0 + 1.0) - 0.1, 0.0, 2.0);
}
