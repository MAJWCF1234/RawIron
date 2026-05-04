#version 450

layout(location = 0) in vec2 vUv;

layout(location = 0) out vec4 fragColor;

layout(std140, set = 0, binding = 0) uniform CameraData {
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
    vec4 directionalLightColorIntensity;
    vec4 viewportMetrics;
} cameraData;

layout(set = 1, binding = 0) uniform sampler2D hdrSceneLinear;
layout(set = 1, binding = 1) uniform sampler2D sceneDepth;

float SampleDepth(vec2 uv01) {
    return texture(sceneDepth, uv01).r;
}

void main() {
    vec2 uv = vec2(vUv.x, 1.0 - vUv.y);
    vec3 linearHdr = texture(hdrSceneLinear, uv).rgb;

    float centerDepth = SampleDepth(uv);
    float aoAccum = 0.0;
    const int kKernel = 8;
    const float radiusPx = 14.0;
    vec2 texel = vec2(cameraData.viewportMetrics.z, cameraData.viewportMetrics.w);

    for (int i = 0; i < kKernel; ++i) {
        float a = float(i) * 0.78539816339 + float(i * i) * 0.231;
        vec2 offset = vec2(cos(a), sin(a)) * (radiusPx * texel);
        float neighborDepth = SampleDepth(uv + offset);
        float delta = centerDepth - neighborDepth;
        if (delta > 0.00005) {
            aoAccum += smoothstep(0.0001, 0.035, delta);
        }
    }

    float ao = clamp(1.0 - aoAccum * 0.11, 0.35, 1.0);
    fragColor = vec4(linearHdr * ao, 1.0);
}
