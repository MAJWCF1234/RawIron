#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec2 texCoord;
layout(location = 3) out vec3 viewDirectionWs;
layout(location = 4) out float viewDistanceWs;
layout(location = 5) out vec4 shadowClipPosition;
layout(location = 6) out vec3 worldPositionWs;

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
    vec4 viewportMetrics;
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
    worldPositionWs = wp.xyz;
    gl_Position = cameraData.viewProjection * wp;
    shadowClipPosition = cameraData.lightViewProjection * wp;
    vec3 v = cameraData.cameraWorldPosition.xyz - wp.xyz;
    float vdLen = length(v);
    viewDirectionWs = vdLen > 1e-5 ? (v / vdLen) : vec3(0.0, 0.0, 1.0);
    viewDistanceWs = vdLen;
    outNormal = normalize(mat3(drawData.model) * inNormal);
    outColor = drawData.color;
    texCoord = inUv * drawData.tiling;
}
