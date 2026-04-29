#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 viewProjection;
    vec4 cameraWorldPosition;
    vec4 renderTuning;
    vec4 postProcessPrimary;
    vec4 postProcessTint;
    vec4 postProcessSecondary;
    mat4 lightViewProjection;
    vec4 lightDirectionIntensity;
    vec4 localLightPositionRange;
    vec4 localLightColorIntensity;
} cameraData;

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
} drawData;

void main() {
    vec4 wp = drawData.model * vec4(inPosition, 1.0);
    gl_Position = cameraData.lightViewProjection * wp;
}
