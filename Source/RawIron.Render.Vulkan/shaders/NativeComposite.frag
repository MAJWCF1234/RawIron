#version 450

layout(location = 0) in vec2 vUv;

layout(location = 0) out vec4 fragColor;

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

layout(set = 1, binding = 0) uniform sampler2D hdrSceneLinear;

vec3 TonemapAcesApprox(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 ApplyColorGrade(vec3 color, float contrast, float saturation) {
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 saturated = mix(vec3(luma), color, saturation);
    return (saturated - 0.5) * contrast + 0.5;
}

float Hash11(float p) {
    return fract(sin(p * 91.3458) * 47453.5453);
}

float Hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec3 ApplyPostProcessFx(vec3 color, vec2 uv01) {
    float noiseAmount = clamp(cameraData.postProcessPrimary.x, 0.0, 0.3);
    float scanlineAmount = clamp(cameraData.postProcessPrimary.y, 0.0, 0.2);
    float barrelDistortion = clamp(cameraData.postProcessPrimary.z, 0.0, 0.2);
    float chromaticAberration = clamp(cameraData.postProcessPrimary.w, 0.0, 0.05);
    vec3 tintColor = clamp(cameraData.postProcessTint.rgb, 0.0, 1.0);
    float tintStrength = clamp(cameraData.postProcessTint.a, 0.0, 1.0);
    float blurAmount = clamp(cameraData.postProcessSecondary.x, 0.0, 0.05);
    float staticFadeAmount = clamp(cameraData.postProcessSecondary.y, 0.0, 1.0);
    float timeSeconds = cameraData.postProcessSecondary.z;

    vec2 centered = uv01 * 2.0 - 1.0;
    float aspect = cameraData.viewportMetrics.x / max(cameraData.viewportMetrics.y, 1.0);
    centered.x *= aspect;
    float radial = clamp(dot(centered, centered), 0.0, 1.0);

    float vignette = 1.0 - (barrelDistortion * radial * radial * 1.6);
    color *= clamp(vignette, 0.0, 1.0);

    float scanPhase = (gl_FragCoord.y + timeSeconds * 48.0) * 0.14;
    float scanline = 1.0 - (scanlineAmount * (0.5 + 0.5 * sin(scanPhase)));
    color *= clamp(scanline, 0.0, 1.0);

    float grainSeed = Hash21(gl_FragCoord.xy + vec2(timeSeconds * 39.0, timeSeconds * 17.0));
    float grain = (grainSeed - 0.5) * noiseAmount * 1.7;
    color = clamp(color + vec3(grain), 0.0, 1.0);

    float fringe = chromaticAberration * radial * 24.0;
    color.r = clamp(color.r + fringe, 0.0, 1.0);
    color.b = clamp(color.b - fringe * 0.75, 0.0, 1.0);

    color = mix(color, color * tintColor, tintStrength);
    float softDesat = clamp(1.0 - blurAmount * 10.0, 0.0, 1.0);
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, softDesat);
    float staticPulse = 0.85 + 0.15 * Hash11(timeSeconds * 23.0 + gl_FragCoord.y * 0.03125);
    color = mix(color, vec3(staticPulse), staticFadeAmount);

    return clamp(color, 0.0, 1.0);
}

void main() {
    // Match NativeScenePreview flipped viewport (positive Y grows downward in framebuffer space).
    vec2 sampleUv = vec2(vUv.x, 1.0 - vUv.y);
    vec3 linearHdr = texture(hdrSceneLinear, sampleUv).rgb;
    float exposure = cameraData.renderTuning.x;
    float contrast = cameraData.renderTuning.y;
    float saturation = cameraData.renderTuning.z;
    vec3 mapped = TonemapAcesApprox(linearHdr);
    mapped = ApplyColorGrade(mapped, contrast, saturation);
    mapped = ApplyPostProcessFx(mapped, sampleUv);
    fragColor = vec4(clamp(mapped, 0.0, 1.0), 1.0);
}
