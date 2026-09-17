#version 450
#extension GL_GOOGLE_include_directive : require
layout(location = 0) in vec2 vUv;
layout(location = 1) flat in float vExposure;
layout(location = 2) flat in float vRodExposure;
layout(location = 0) out vec4 fragColor;
#include "SerenityColorCommon.glsl"
#include "SerenityRadiance.glsl"

void main() {
    vec2 uv = vec2(vUv.x, 1.0 - vUv.y);
    vec3 color = max(texture(hdrSceneLinear, uv).rgb, vec3(0.0));
    color = SerenityPrepareRadiance(color, uv, vExposure, vRodExposure);
    // Bliss composite11 display transform after native radiance prepare.
    if (serenity.controls[5].z > .5) {
        color = color * ACESInputMat;
        color = SerenityToneMap(color);
        color = LinearTosRGB(clamp(color * ACESOutputMat, 0.0, 1.0));
    } else color = LinearTosRGB(SerenityToneMap(color));
    fragColor = vec4(clamp(SerenityDither(SerenityFinite(color), uv), 0.0, 1.0), 1.0);
}
