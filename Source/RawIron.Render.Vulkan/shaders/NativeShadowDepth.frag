#version 450

layout(location = 0) in vec2 texCoord;

layout(push_constant) uniform DrawData {
    layout(offset = 0) mat4 model;
    layout(offset = 64) vec4 color;
    layout(offset = 80) vec2 tiling;
    layout(offset = 88) int useTexture;
    layout(offset = 92) int nativeWaterUvMotion;
    layout(offset = 96) float nativeWaterTime;
    layout(offset = 100) int litShadingModel;
    layout(offset = 104) float metallic;
    layout(offset = 108) float roughness;
    layout(offset = 112) vec3 emissiveColor;
    layout(offset = 124) float qualityTier;
    layout(offset = 128) float alphaCutoff;
} drawData;

layout(set = 1, binding = 0) uniform sampler2D albedoTex;
layout(set = 1, binding = 4) uniform sampler2D opacityTex;

const int kMaterialAlbedoAlphaSmoothness = 1 << 14;

void main() {
    if ((drawData.litShadingModel & 2) == 0) {
        return;
    }
    if (drawData.useTexture == 0) {
        return;
    }

    vec2 uv = texCoord * drawData.tiling;
    float alpha = 1.0;
    if ((drawData.litShadingModel & kMaterialAlbedoAlphaSmoothness) != 0) {
        alpha = texture(opacityTex, uv).r;
    } else {
        vec4 albedoSample = texture(albedoTex, uv);
        alpha = albedoSample.a * texture(opacityTex, uv).r;
    }
    if (alpha < drawData.alphaCutoff) {
        discard;
    }
}
