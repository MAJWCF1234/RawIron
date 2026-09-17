// Serenity native color stage. Tone-map and composite include this file.
// Radiance/TAA/sky/water stages live in engine shaders; ThirdParty/Bliss is hashed reference only.
// set 1 binding 0 is pass-local: HDR scene for tone map, tone-mapped color for composite.
layout(std140, set = 0, binding = 0) uniform SerenityCameraData {
    layout(offset = 0) mat4 viewProjection;
    layout(offset = 64) vec4 cameraWorldPosition;
    layout(offset = 208) vec4 lightDirectionIntensity;
    layout(offset = 256) vec4 directionalLightColorIntensity;
    layout(offset = 4304) vec4 controls[9];
    layout(offset = 4448) mat4 previousViewProjection;
    layout(offset = 4512) mat4 invViewProjection;
    // seconds since submission, valid history, submitted frame counter, reserved
    layout(offset = 4576) vec4 temporalState;
    layout(offset = 4592) vec4 exposureTuning;
    layout(offset = 4608) vec4 purkinjeColor;
} serenity;
layout(set = 1, binding = 0) uniform sampler2D hdrSceneLinear;

#define frameTimeCounter serenity.controls[6].z
#include "SerenityColorTransforms.glsl"
#include "SerenityColorDither.glsl"

vec3 SerenityFinite(vec3 value) {
    // Undefined upstream log/pow/dither edge cases must never poison the target.
    return vec3(isnan(value.x) || isinf(value.x) ? 0.0 : value.x,
                isnan(value.y) || isinf(value.y) ? 0.0 : value.y,
                isnan(value.z) || isinf(value.z) ? 0.0 : value.z);
}
vec3 SerenityDither(vec3 color, vec2 uv) {
    return serenity.controls[1].z > .5 ? SerenityFinite(int8Dither(color, uv)) : color;
}
vec3 SerenityToneMap(vec3 color) {
    switch (int(serenity.controls[0].w + .5)) {
        case 1: return ToneMap_AgX(color);
        case 2: return ToneMap_Hejl2015(color);
        case 3: return Tonemap_Xonk(color);
        case 4: return Tonemap_Uchimura(color);
        case 5: return HableTonemap(color);
        case 6: return Full_Reinhard_Edit(color);
        case 7: return Tonemap_Full_Reinhard(color);
        case 8: return reinhard(color);
        case 9: return Tonemap_Lottes(color);
        case 10: return ACESFilm(color);
        default: return ToneMap_AgX_minimal(color);
    }
}
