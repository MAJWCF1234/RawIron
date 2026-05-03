#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 viewDirectionWs;
layout(location = 4) in float viewDistanceWs;
layout(location = 5) in vec4 shadowClipPosition;
layout(location = 6) in vec3 worldPositionWs;

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
} cameraData;

layout(set = 1, binding = 0) uniform sampler2D albedoTex;
layout(set = 2, binding = 0) uniform sampler2D shadowMapTex;

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

const float kPi = 3.14159265359;

float DistributionGGX(vec3 n, vec3 h, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(n, h), 0.0);
    float nDotH2 = nDotH * nDotH;
    float d = (nDotH2 * (a2 - 1.0)) + 1.0;
    return a2 / max(kPi * d * d, 1e-5);
}

float GeometrySchlickGGX(float nDotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) * 0.125;
    return nDotV / max((nDotV * (1.0 - k)) + k, 1e-5);
}

float GeometrySmith(vec3 n, vec3 v, vec3 l, float roughness) {
    float ggx2 = GeometrySchlickGGX(max(dot(n, v), 0.0), roughness);
    float ggx1 = GeometrySchlickGGX(max(dot(n, l), 0.0), roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

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

vec3 ApplyPostProcessFx(vec3 color) {
    float noiseAmount = clamp(cameraData.postProcessPrimary.x, 0.0, 0.3);
    float scanlineAmount = clamp(cameraData.postProcessPrimary.y, 0.0, 0.2);
    float barrelDistortion = clamp(cameraData.postProcessPrimary.z, 0.0, 0.2);
    float chromaticAberration = clamp(cameraData.postProcessPrimary.w, 0.0, 0.05);
    vec3 tintColor = clamp(cameraData.postProcessTint.rgb, 0.0, 1.0);
    float tintStrength = clamp(cameraData.postProcessTint.a, 0.0, 1.0);
    float blurAmount = clamp(cameraData.postProcessSecondary.x, 0.0, 0.05);
    float staticFadeAmount = clamp(cameraData.postProcessSecondary.y, 0.0, 1.0);
    float timeSeconds = cameraData.postProcessSecondary.z;

    vec2 uv = fract(gl_FragCoord.xy * 0.002 + vec2(0.173, 0.391));
    vec2 centered = uv * 2.0 - 1.0;
    float radial = clamp(dot(centered, centered), 0.0, 1.0);

    // Improvement 1: radial vignette from barrel-distortion channel.
    float vignette = 1.0 - (barrelDistortion * radial * radial * 1.6);
    color *= clamp(vignette, 0.0, 1.0);

    // Improvement 2: animated scanline intensity.
    float scanPhase = (gl_FragCoord.y + timeSeconds * 48.0) * 0.14;
    float scanline = 1.0 - (scanlineAmount * (0.5 + 0.5 * sin(scanPhase)));
    color *= clamp(scanline, 0.0, 1.0);

    // Improvement 3: temporal film grain.
    float grainSeed = Hash21(gl_FragCoord.xy + vec2(timeSeconds * 39.0, timeSeconds * 17.0));
    float grain = (grainSeed - 0.5) * noiseAmount * 1.7;
    color = clamp(color + vec3(grain), 0.0, 1.0);

    // Improvement 4: cheap chromatic fringing near screen edges.
    float fringe = chromaticAberration * radial * 24.0;
    color.r = clamp(color.r + fringe, 0.0, 1.0);
    color.b = clamp(color.b - fringe * 0.75, 0.0, 1.0);

    // Improvement 5: tint + static fade + blur proxy for a softer blend.
    color = mix(color, color * tintColor, tintStrength);
    float softDesat = clamp(1.0 - blurAmount * 10.0, 0.0, 1.0);
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, softDesat);
    float staticPulse = 0.85 + 0.15 * Hash11(timeSeconds * 23.0 + gl_FragCoord.y * 0.03125);
    color = mix(color, vec3(staticPulse), staticFadeAmount);

    return clamp(color, 0.0, 1.0);
}

float ComputeShadowFactor(vec3 normal, vec3 lightDir) {
    vec3 ndc = shadowClipPosition.xyz / max(shadowClipPosition.w, 1e-5);
    vec2 uv = ndc.xy * 0.5 + 0.5;
    float receiverDepth = ndc.z * 0.5 + 0.5;
    if (uv.x <= 0.0 || uv.y <= 0.0 || uv.x >= 1.0 || uv.y >= 1.0 || receiverDepth <= 0.0 || receiverDepth >= 1.0) {
        return 1.0;
    }
    float bias = max(0.0008, 0.0022 * (1.0 - max(dot(normal, lightDir), 0.0)));
    vec2 texel = 1.0 / vec2(textureSize(shadowMapTex, 0));
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float sampleDepth = texture(shadowMapTex, uv + vec2(float(x), float(y)) * texel).r;
            lit += (receiverDepth - bias) <= sampleDepth ? 1.0 : 0.0;
        }
    }
    return lit / 9.0;
}

void main() {
    const bool hybridHdrRadiance = cameraData.postProcessSecondary.w > 0.5;
    vec3 normal = normalize(inNormal);
    vec3 albedo = inColor.rgb;
    float sampledAlpha = 1.0;
    if (drawData.useTexture != 0) {
        vec2 uv = texCoord;
        if (drawData.nativeWaterUvMotion != 0) {
            const float t = drawData.nativeWaterTime;
            const float s = 0.018 / max(drawData.tiling.x, 0.25);
            uv += vec2(
                sin(t * 1.15 + texCoord.y * 5.5) * s,
                cos(t * 0.95 + texCoord.x * 4.8) * s
            );
        }
        vec4 texel = texture(albedoTex, uv);
        albedo *= texel.rgb;
        sampledAlpha = texel.a;
    }
    albedo = clamp(albedo, 0.0, 1.0);
    bool alphaCutout = (drawData.litShadingModel & 2) != 0;
    float outputAlpha = inColor.a * sampledAlpha;
    if (alphaCutout) {
        if (sampledAlpha < inColor.a) {
            discard;
        }
        outputAlpha = 1.0;
    }

    float tier = clamp(drawData.qualityTier, 0.0, 2.0);
    float exposure = cameraData.renderTuning.x;
    float contrast = cameraData.renderTuning.y;
    float saturation = cameraData.renderTuning.z;
    float fogDensity = cameraData.renderTuning.w;

    if ((drawData.litShadingModel & 1) == 0) {
        vec3 linearUnlit = (albedo + drawData.emissiveColor) * exposure;
        if (hybridHdrRadiance) {
            fragColor = vec4(linearUnlit, outputAlpha);
            return;
        }
        vec3 unlit = TonemapAcesApprox(linearUnlit);
        unlit = ApplyColorGrade(unlit, contrast, saturation);
        unlit = ApplyPostProcessFx(unlit);
        fragColor = vec4(clamp(unlit, 0.0, 1.0), outputAlpha);
        return;
    }

    vec3 lightDir = normalize(cameraData.lightDirectionIntensity.xyz);
    float sunIntensity = max(cameraData.lightDirectionIntensity.w, 0.0);
    vec3 lightColor = vec3(1.0, 0.98, 0.94) * sunIntensity;
    float roughness = clamp(drawData.roughness, 0.04, 1.0);
    float metallic = clamp(drawData.metallic, 0.0, 1.0);
    vec3 V = viewDirectionWs;
    vec3 H = normalize(lightDir + V);
    float nDotL = max(dot(normal, lightDir), 0.0);
    float nDotV = max(dot(normal, V), 0.0);
    float hDotV = max(dot(H, V), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = FresnelSchlick(hDotV, F0);
    float D = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(normal, V, lightDir, roughness);
    vec3 specular = vec3(0.0);
    if (nDotL > 1e-4 && nDotV > 1e-4) {
        specular = (D * G * F) / max(4.0 * nDotV * nDotL, 1e-4);
    }

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    float diffuseTerm = mix(max(nDotL, 0.06), nDotL, 0.75);
    vec3 diffuse = (kD * albedo / kPi) * diffuseTerm;
    float shadow = ComputeShadowFactor(normal, lightDir);
    vec3 direct = (diffuse + (specular * max(nDotL, 0.0))) * lightColor * 1.65 * shadow;
    vec3 ambient = albedo * vec3(0.048, 0.052, 0.060) * (1.0 - metallic * 0.45);
    if (tier >= 1.0) {
        float hemi = normal.y * 0.5 + 0.5;
        vec3 sky = vec3(0.19, 0.24, 0.30);
        vec3 ground = vec3(0.08, 0.072, 0.066);
        ambient += albedo * mix(ground, sky, hemi) * 0.12;
    }
    if (tier >= 2.0) {
        vec3 coatF0 = vec3(0.04);
        vec3 coatF = FresnelSchlick(hDotV, coatF0);
        float coatD = DistributionGGX(normal, H, 0.15);
        float coatG = GeometrySmith(normal, V, lightDir, 0.15);
        vec3 clearcoat = (coatD * coatG * coatF) / max(4.0 * nDotV * nDotL, 1e-4);
        direct += clearcoat * nDotL * 0.14;
    }
    vec3 litRgb = ambient + direct + drawData.emissiveColor;
    vec3 toLocal = cameraData.localLightPositionRange.xyz - worldPositionWs;
    float localDistance = length(toLocal);
    float localRange = max(cameraData.localLightPositionRange.w, 0.001);
    vec3 localDir = localDistance > 1e-4 ? (toLocal / localDistance) : vec3(0.0, 1.0, 0.0);
    float localNdotL = max(dot(normal, localDir), 0.0);
    float localAtten = clamp(1.0 - (localDistance / localRange), 0.0, 1.0);
    localAtten *= localAtten;
    vec3 localColor = cameraData.localLightColorIntensity.rgb * cameraData.localLightColorIntensity.w;
    litRgb += albedo * localColor * localNdotL * localAtten * 0.55;
    litRgb = max(litRgb, albedo * 0.018);

    float fogAmount = clamp(1.0 - exp2(-viewDistanceWs * fogDensity), 0.0, 1.0);
    float horizonFactor = clamp(1.0 - max(normalize(viewDirectionWs).y, 0.0), 0.0, 1.0);
    vec3 fogColorNear = vec3(0.34, 0.39, 0.42);
    vec3 fogColorFar = vec3(0.40, 0.47, 0.54);
    vec3 fogColor = mix(fogColorNear, fogColorFar, horizonFactor);
    vec3 color = mix(litRgb, fogColor, fogAmount * 0.28);
    vec3 linearLit = color * exposure;
    if (hybridHdrRadiance) {
        fragColor = vec4(linearLit, outputAlpha);
        return;
    }
    vec3 mapped = TonemapAcesApprox(linearLit);
    mapped = ApplyColorGrade(mapped, contrast, saturation);
    mapped = ApplyPostProcessFx(mapped);
    fragColor = vec4(clamp(mapped, 0.0, 1.0), outputAlpha);
}
