#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 viewDirectionWs;
layout(location = 4) in float viewDistanceWs;
layout(location = 5) in vec4 shadowClipPosition;
layout(location = 6) in vec3 worldPositionWs;

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
    vec4 presentationTuning;
    vec4 presentationColorGrading;
    vec4 presentationExtra;
    vec4 lggLiftMix;
    vec4 lggGammaRgb;
    vec4 lggGainRgb;
    vec4 vibranceBalanceAmount;
    vec4 technicolor1PowStrNegRg;
    vec4 technicolor1NegBPad;
    vec4 technicolor2ColBright;
    vec4 technicolor2SatStrPad;
    vec4 sepiaTintXyzStrength;
    vec4 monochromePresetSat;
    vec4 monochromeCustomCoeff;
    vec4 dpxRgbCurvePad;
    vec4 dpxRgbCPad;
    vec4 dpxContrastSatColorStr;
    vec4 colorMatrixRowR;
    vec4 colorMatrixRowG;
    vec4 colorMatrixRowBStr;
    vec4 fakeHdrPowerR1R2Str;
    vec4 levelsBlackWhiteStrClip;
    vec4 lumaSharpenPack;
    vec4 sweetFxCurvesPack;
    vec4 sweetFxChromaticAberrationPack;
    vec4 sweetFxBorderPack;
    vec4 sweetFxBorderColorPad;
    vec4 sweetFxCartoonPack;
    vec4 sweetFxTonemapGammaExpSatBleach;
    vec4 sweetFxTonemapFogColorDefog;
    vec4 sweetFxTonemapStrengthPad;
    vec4 sweetFxSplitscreenModeStrength;
    vec4 sweetFxNostalgiaPack;
    vec4 sweetFxComparePack;
    vec4 sweetFxLayerPosScaleBlend;
    vec4 sweetFxLayerTexSizePad;
    vec4 sweetFxFxaaPack;
    vec4 sweetFxCrtPack0;
    vec4 sweetFxCrtPack1;
    vec4 sweetFxCrtPack2;
    vec4 sweetFxCrtPack3;
    vec4 sweetFxAsciiPack0;
    vec4 sweetFxAsciiPack1;
    vec4 sweetFxAsciiPack2;
    vec4 sweetFxAsciiFontColorPad;
    vec4 sweetFxAsciiBackgroundColorPad;
    vec4 sweetFxSmaaPack0;
    vec4 sweetFxSmaaPack1;
    vec4 reshadeDaltonizePack;
    vec4 reshadeDisplayDepthPack;
    vec4 reshadeLutPack;
    vec4 pd80TcRedStrPad;
    vec4 pd80TcCyanPad;
    vec4 pd80TcKeySat2Pad;
    vec4 pd80Tc3ColBrightPad;
    vec4 pd80Tc3SatStrEnPad;
    vec4 pd80ColorTempKelvinLumMixStr;
    vec4 pd80SatLimitCapStr;
    vec4 pd80ColorBalanceShadowPad;
    vec4 pd80ColorBalanceMidPad;
    vec4 pd80ColorBalanceHighPad;
    vec4 pd80ColorBalanceOptStr;
    vec4 pd80ColorIsolationHueRangeSatMix;
    vec4 pd80ColorIsolationStrPad;
    vec4 pd80LevelsIbPad;
    vec4 pd80LevelsIwPad;
    vec4 pd80LevelsObPad;
    vec4 pd80LevelsOwPad;
    vec4 pd80LevelsGammaDitherStr;
    vec4 pd80BwPack0;
    vec4 pd80BwPack1;
    vec4 pd80BwPack2;
    vec4 pd80BwPack3;
    vec4 pd80CbsPack0;
    vec4 pd80CbsPack1;
    vec4 pd80CbsPack2;
    vec4 pd80CbsPack3;
    vec4 pd80CbsPack4;
    vec4 pd80CbsPack5;
    vec4 pd80CbsPack6;
    vec4 pd80CbsPack7;
    vec4 pd80CaPack0;
    vec4 pd80CaPack1;
    vec4 pd80CaPack2;
    vec4 pd80CaPack3;
    vec4 pd80CaPack4;
    vec4 pd80CaPack5;
    vec4 pd80LsPack0;
    vec4 pd80LsPack1;
    vec4 pd80LsPack2;
    vec4 pd80FgPack0;
    vec4 pd80FgPack1;
    vec4 pd80FgPack2;
    vec4 pd80FgPack3;
    vec4 pd80FgPack4;
    vec4 pd80DsPack0;
    vec4 pd80DsPack1;
    vec4 pd80DsPack2;
    vec4 pd80CgPack0;
    vec4 pd80CscPack0;
    vec4 pd80CscPack1;
    vec4 pd80CscPack2;
    vec4 pd80SmhPack0;
    vec4 pd80SmhPack1;
    vec4 pd80SmhPack2;
    vec4 pd80SmhPack3;
    vec4 pd80SmhPack4;
    vec4 pd80SmhPack5;
    vec4 pd80SmhPack6;
    vec4 pd80SmhPack7;
    vec4 pd80SmhPack8;
    vec4 pd80SmhPack9;
    vec4 pd80SmhPack10;
    vec4 pd80ClPack0;
    vec4 pd80ClPack1;
    vec4 pd80ClPack2;
    vec4 pd80ClPack3;
    vec4 pd80ClPack4;
    vec4 pd80ClPack5;
    vec4 pd80ClPack6;
    vec4 pd80ClPack7;
    vec4 pd80ClPack8;
    vec4 pd80ScPack0;
    vec4 pd80ScPack1;
    vec4 pd80ScPack2;
    vec4 pd80ScPack3;
    vec4 pd80ScPack4;
    vec4 pd80ScPack5;
    vec4 pd80ScPack6;
    vec4 pd80ScPack7;
    vec4 pd80ScPack8;
    vec4 pd80ScPack9;
    vec4 pd80ScPack10;
    vec4 pd80ScPack11;
    vec4 pd80ScPack12;
    vec4 pd80ScPack13;
    vec4 pd80ScPack14;
    vec4 pd80ScPack15;
    vec4 pd80ScPack16;
    vec4 pd80ScPack17;
    vec4 pd80ScPack18;
    vec4 pd80PpPack0;
    vec4 pd80PpPack1;
    vec4 pd80MrPack0;
    vec4 pd80MrPack1;
    vec4 pd80MrPack2;
    vec4 pd80MrPack3;
    vec4 pd80MrPack4;
    vec4 pd80MrPack5;
    vec4 pd80MrPack6;
    vec4 pd80MrPack7;
    vec4 pd80BlpPack0;
    vec4 pd80BlpPack1;
    vec4 pd80BlpPack2;
    vec4 pd80BlpPack3;
    vec4 pd80BlpPack4;
    vec4 pd80BlpPack5;
    vec4 pd80CltPack0;
    vec4 pd80CltPack1;
    vec4 pd80CltPack2;
    vec4 pd80CltPack3;
    vec4 pd80CltPack4;
    vec4 pd80CltPack5;
    vec4 pd80LcPack0;
    vec4 pd80LfPack0;
    vec4 pd80Cg4Pack0;
    vec4 pd80Cg4Pack1;
    vec4 pd80Cg4Pack2;
    vec4 pd80Cg4Pack3;
    vec4 pd80Cg4Pack4;
    vec4 pd80Cg4Pack5;
    vec4 pd80Cg4Pack6;
    vec4 pd80Cg4Pack7;
    vec4 pd80Cg4Pack8;
    vec4 pd80CcPack0;
    vec4 pd80RccPack0;
    vec4 pd80RccPack1;
    vec4 pd80RccPack2;
    vec4 pd80RccPack3;
    vec4 pd80RccPack4;
    vec4 pd80FaPack0;
    vec4 pd80HbPack0;
    vec4 pd80HbPack1;
    vec4 pd80HbPack2;
    vec4 pd80Sc2Pack0;
} cameraData;

layout(set = 1, binding = 0) uniform sampler2D albedoTex;
layout(set = 1, binding = 1) uniform sampler2D normalTex;
layout(set = 1, binding = 2) uniform sampler2D ormTex;
layout(set = 1, binding = 3) uniform sampler2D emissiveTex;
layout(set = 1, binding = 4) uniform sampler2D opacityTex;
layout(set = 1, binding = 5) uniform sampler2D detailTex;
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
const int kMaterialStyleRetro = 1 << 5;
const int kMaterialStyleLayered = 1 << 6;
const int kMaterialStyleMixedMedia = 1 << 7;
const int kMaterialStyleCrystal = 1 << 8;
const int kMaterialWorkflowSpecGloss = 1 << 9;

float Hash11(float p);
float Hash21(vec2 p);

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

mat3 CotangentFrame(vec3 n, vec3 positionWs, vec2 uv) {
    vec3 dp1 = dFdx(positionWs);
    vec3 dp2 = dFdy(positionWs);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, n);
    vec3 dp1perp = cross(n, dp1);
    vec3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
    float invMax = inversesqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
    return mat3(tangent * invMax, bitangent * invMax, n);
}

vec3 ApplyNormalMap(vec3 baseNormal, vec2 uv) {
    vec3 tangentNormal = texture(normalTex, uv).xyz * 2.0 - 1.0;
    tangentNormal.xy *= 0.9;
    mat3 tbn = CotangentFrame(normalize(baseNormal), worldPositionWs, uv);
    return normalize(tbn * tangentNormal);
}

vec3 ComputePseudoReflection(vec3 normal, vec3 viewDir, float roughness, vec3 ambientBoost) {
    vec3 reflected = reflect(-viewDir, normal);
    float horizon = clamp(reflected.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky = mix(vec3(0.08, 0.11, 0.15), vec3(0.46, 0.62, 0.88) + ambientBoost * 0.12, horizon);
    vec3 ground = vec3(0.05, 0.045, 0.055) + ambientBoost * 0.04;
    vec3 env = mix(ground, sky, horizon);
    float reflectStrength = clamp(1.0 - roughness, 0.0, 1.0);
    return env * (0.18 + reflectStrength * reflectStrength * 0.72);
}

vec3 ApplyRetroStyle(vec3 color, vec2 uv, float timeSeconds) {
    vec3 quantized = floor(color * 6.0 + 0.5) / 6.0;
    float dither = (Hash21(floor(gl_FragCoord.xy) + vec2(timeSeconds * 8.0, uv.y * 37.0)) - 0.5) * 0.075;
    return clamp(quantized + dither, 0.0, 1.0);
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

float SweetFxSrgbLumaCurve(float x) {
    x = clamp(x, 0.0, 1.0);
    return x * (x * (1.5 - x) + 0.5);
}

vec3 ApplySweetFxLumaCurve(vec3 rgb, float strength) {
    float s = clamp(strength, 0.0, 1.0);
    if (s < 1e-6) {
        return rgb;
    }
    float lum = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    float curved = SweetFxSrgbLumaCurve(lum);
    float newLum = mix(lum, curved, s);
    return rgb * (newLum / max(lum, 1e-5));
}

float TriD_Rand11(float x) {
    return fract(x * 0.024390243);
}
float TriD_Permute(float x) {
    return mod((34.0 * x + 1.0) * x, 289.0);
}
float TriD_Rand21(vec2 uv) {
    float a = fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
    float b = fract(sin(dot(uv, vec2(39.346, 11.135))) * 26974.324);
    return (a + b) * 0.5;
}

vec3 TriangularDitherRgb(vec3 color, vec2 uv01, float timeSeconds, float strength) {
    if (strength < 1e-5) {
        return color;
    }
    const int kBits = 8;
    float bitstep = exp2(float(kBits)) - 1.0;
    float lsb = 1.0 / bitstep;
    float lobit = 0.5 / bitstep;
    float hibit = (bitstep - 0.5) / bitstep;

    vec3 m = vec3(uv01, TriD_Rand21(uv01 + timeSeconds * 0.001)) + 1.0;
    float h = TriD_Permute(TriD_Permute(TriD_Permute(m.x) + m.y) + m.z);

    float n1x = TriD_Rand11(h);
    h = TriD_Permute(h);
    float n2x = TriD_Rand11(h);
    h = TriD_Permute(h);
    float n1y = TriD_Rand11(h);
    h = TriD_Permute(h);
    float n2y = TriD_Rand11(h);
    h = TriD_Permute(h);
    float n1z = TriD_Rand11(h);
    h = TriD_Permute(h);
    float n2z = TriD_Rand11(h);
    vec3 noise1 = vec3(n1x, n1y, n1z);
    vec3 noise2 = vec3(n2x, n2y, n2z);

    vec3 lo = clamp(color / lobit, 0.0, 1.0);
    vec3 hi = clamp((color - 1.0) / (hibit - 1.0), 0.0, 1.0);
    vec3 uni = noise1 - 0.5;
    vec3 tri = noise1 - noise2;
    vec3 dith = mix(uni, tri, min(lo, hi)) * lsb;
    return color + dith * clamp(strength, 0.0, 1.0);
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

vec3 CalibrateRoughStoneColor(vec3 color, float roughStone, bool layeredStyle, bool mixedMediaStyle) {
    float stoneWeight = roughStone * (layeredStyle ? 1.0 : (mixedMediaStyle ? 0.42 : 0.0));
    if (stoneWeight <= 1e-4) {
        return color;
    }
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 neutral = vec3(luma);
    color = mix(color, neutral, 0.46 * stoneWeight);
    float greenExcess = max(color.g - max(color.r, color.b), 0.0);
    color.g -= greenExcess * 0.72 * stoneWeight;
    float cyanExcess = max(color.b - color.r * 1.08, 0.0);
    color.b -= cyanExcess * 0.24 * stoneWeight;
    return max(color, vec3(0.0));
}

float ComputeShadowFactor(vec3 normal, vec3 lightDir) {
    vec3 ndc = shadowClipPosition.xyz / max(shadowClipPosition.w, 1e-5);
    vec2 uv = ndc.xy * 0.5 + 0.5;
    float receiverDepth = ndc.z * 0.5 + 0.5;
    if (uv.x <= 0.0 || uv.y <= 0.0 || uv.x >= 1.0 || uv.y >= 1.0 || receiverDepth <= 0.0 || receiverDepth >= 1.0) {
        return 1.0;
    }
    const float tier = clamp(drawData.qualityTier, 0.0, 2.0);
    float bias = max(0.0008, 0.0022 * (1.0 - max(dot(normal, lightDir), 0.0)));
    if (tier >= 2.0) {
        bias *= 0.92;
    }
    vec2 texel = 1.0 / vec2(textureSize(shadowMapTex, 0));
    float lit = 0.0;
    if (tier >= 2.0) {
        for (int y = -2; y <= 2; ++y) {
            for (int x = -2; x <= 2; ++x) {
                float sampleDepth = texture(shadowMapTex, uv + vec2(float(x), float(y)) * texel).r;
                lit += (receiverDepth - bias) <= sampleDepth ? 1.0 : 0.0;
            }
        }
        return lit / 25.0;
    }
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
    const vec2 postUv01 = gl_FragCoord.xy * cameraData.viewportMetrics.zw;
    vec2 materialUv = texCoord * drawData.tiling;
    vec2 detailUv = materialUv * 1.85 + worldPositionWs.xz * 0.028;
    vec3 normal = normalize(inNormal);
    const vec3 viewDir = normalize(viewDirectionWs);
    if ((drawData.litShadingModel & 8) != 0 && dot(normal, viewDir) < 0.0) {
        normal = -normal;
    }
    const bool retroStyle = (drawData.litShadingModel & kMaterialStyleRetro) != 0;
    const bool layeredStyle = (drawData.litShadingModel & kMaterialStyleLayered) != 0;
    const bool mixedMediaStyle = (drawData.litShadingModel & kMaterialStyleMixedMedia) != 0;
    const bool crystalStyle = (drawData.litShadingModel & kMaterialStyleCrystal) != 0;
    const bool specGlossWorkflow = (drawData.litShadingModel & kMaterialWorkflowSpecGloss) != 0;
    vec3 albedo = inColor.rgb;
    float sampledAlpha = 1.0;
    vec3 emissiveTexel = vec3(0.0);
    vec3 ormTexel = vec3(1.0, drawData.roughness, drawData.metallic);
    if (drawData.useTexture != 0) {
        vec2 uv = materialUv;
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
        sampledAlpha = texel.a * texture(opacityTex, uv).r;
        normal = ApplyNormalMap(normal, uv);
        ormTexel = texture(ormTex, uv).rgb;
        emissiveTexel = texture(emissiveTex, uv).rgb;
        if (layeredStyle || mixedMediaStyle || crystalStyle) {
            vec3 detailSample = texture(detailTex, detailUv).rgb;
            float macroNoise = Hash21(floor(worldPositionWs.xz * 0.75) + vec2(worldPositionWs.y * 0.15));
            float roughStoneDetail = smoothstep(0.86, 0.98, drawData.roughness) * (1.0 - drawData.metallic);
            float detailMix = layeredStyle ? 0.16 : (mixedMediaStyle ? 0.13 : 0.09);
            detailMix = mix(detailMix, max(detailMix, 0.18), roughStoneDetail);
            vec3 coolWarmTint = mix(vec3(0.92, 0.95, 1.0), vec3(1.06, 1.0, 0.94), macroNoise);
            vec3 stoneTint = mix(vec3(0.98, 0.97, 0.95), vec3(1.04, 1.02, 0.98), macroNoise);
            vec3 macroTint = mix(coolWarmTint, stoneTint, roughStoneDetail);
            float detailLuma = dot(detailSample, vec3(0.2126, 0.7152, 0.0722));
            vec3 detailContrast = vec3(1.0 + (detailLuma - 0.52) * mix(0.18, 0.30, roughStoneDetail));
            vec2 grainUv = mix(worldPositionWs.xy, worldPositionWs.xz, abs(normal.y));
            float grainA = Hash21(floor(grainUv * 3.0 + worldPositionWs.zy * 0.17));
            float grainB = Hash21(floor(grainUv * 9.0 + vec2(worldPositionWs.z, worldPositionWs.x) * 0.12));
            float pore = smoothstep(0.64, 0.96, grainB) * 0.08;
            detailContrast *= vec3(1.0 + (grainA - 0.5) * 0.08 - pore);
            albedo *= mix(vec3(1.0), detailContrast * macroTint, detailMix);
        }
    }
    albedo = clamp(albedo, 0.0, 1.0);
    bool alphaCutout = (drawData.litShadingModel & 2) != 0;
    float outputAlpha = inColor.a * sampledAlpha;
    if (alphaCutout) {
        const float cutoff = inColor.a;
        const float feather = 0.20;
        const float coverage = smoothstep(cutoff - feather, cutoff + feather * 0.45, sampledAlpha);
        if (coverage < 0.004) {
            discard;
        }
        albedo *= coverage;
        outputAlpha = 1.0;
    }

    float tier = clamp(drawData.qualityTier, 0.0, 2.0);
    float exposure = cameraData.renderTuning.x;
    float contrast = cameraData.renderTuning.y;
    float saturation = cameraData.renderTuning.z;
    float fogDensity = cameraData.renderTuning.w;
    float curveAmt = clamp(cameraData.presentationColorGrading.x, 0.0, 1.0);
    float ditherAmt = clamp(cameraData.presentationColorGrading.y, 0.0, 1.0);
    float debandAmt = clamp(cameraData.presentationColorGrading.z, 0.0, 0.12);

    if ((drawData.litShadingModel & 1) == 0) {
        // Bit 2 (value 4): soft additive sprites — radial falloff + mild distance fade.
        if ((drawData.litShadingModel & 4) != 0) {
            vec2 centered = texCoord - vec2(0.5);
            float radial = clamp(1.0 - dot(centered, centered) * 4.0, 0.0, 1.0);
            radial *= radial;
            if (drawData.useTexture != 0) {
                radial *= sampledAlpha;
            }
            outputAlpha *= radial;
            float distFade = clamp(1.0 - (viewDistanceWs - 1.5) / 72.0, 0.12, 1.0);
            outputAlpha *= distFade;
        }
        vec3 linearUnlit = (albedo + drawData.emissiveColor + emissiveTexel) * exposure;
        if (hybridHdrRadiance) {
            if (retroStyle) {
                linearUnlit = ApplyRetroStyle(linearUnlit, postUv01, cameraData.postProcessSecondary.z);
            }
            fragColor = vec4(linearUnlit, outputAlpha);
            return;
        }
        vec3 unlit = TonemapAcesApprox(linearUnlit);
        unlit = ApplyColorGrade(unlit, contrast, saturation);
        if (retroStyle) {
            unlit = ApplyRetroStyle(unlit, postUv01, cameraData.postProcessSecondary.z);
        }
        unlit = ApplySweetFxLumaCurve(unlit, curveAmt);
        unlit = ApplyPostProcessFx(unlit, postUv01);
        if (debandAmt > 1e-5) {
            float dl = dot(unlit, vec3(0.2126, 0.7152, 0.0722));
            vec2 grad = vec2(dFdx(dl), dFdy(dl));
            float gradMag = clamp(length(grad) * 96.0, 0.0, 1.0);
            float rnd = Hash21(gl_FragCoord.xy + vec2(cameraData.postProcessSecondary.z * 13.7, cameraData.postProcessSecondary.z * 9.1)) - 0.5;
            unlit += rnd * debandAmt * (0.12 + gradMag * 0.55);
        }
        unlit = TriangularDitherRgb(unlit, postUv01, cameraData.postProcessSecondary.z, ditherAmt);
        fragColor = vec4(clamp(unlit, 0.0, 1.0), outputAlpha);
        return;
    }

    vec3 lightDir = normalize(cameraData.lightDirectionIntensity.xyz);
    float sunDimmer = max(cameraData.lightDirectionIntensity.w, 0.0);
    vec3 lightColor = cameraData.directionalLightColorIntensity.rgb * sunDimmer;
    vec3 specColor = vec3(0.04);
    float ao = 1.0;
    float roughness = drawData.roughness;
    float metallic = drawData.metallic;
    if (specGlossWorkflow) {
        float specStrength = clamp(max(max(ormTexel.r, ormTexel.g), ormTexel.b), 0.0, 1.0);
        specColor = mix(vec3(0.04), clamp(ormTexel.rgb, 0.0, 1.0), specStrength);
        roughness = clamp(mix(drawData.roughness, 1.0 - specStrength, 0.82), 0.04, 1.0);
        metallic = clamp(max(drawData.metallic, specStrength * 0.35), 0.0, 1.0);
        ao = clamp(0.78 + (1.0 - specStrength) * 0.22, 0.2, 1.0);
    } else {
        roughness = clamp(mix(drawData.roughness, ormTexel.g, 0.85), 0.04, 1.0);
        metallic = clamp(mix(drawData.metallic, ormTexel.b, 0.85), 0.0, 1.0);
        ao = clamp(ormTexel.r, 0.2, 1.0);
        specColor = mix(vec3(0.04), albedo, metallic);
    }
    vec3 V = viewDir;
    vec3 H = normalize(lightDir + V);
    float nDotL = max(dot(normal, lightDir), 0.0);
    float nDotV = max(dot(normal, V), 0.0);
    float hDotV = max(dot(H, V), 0.0);

    vec3 F0 = specGlossWorkflow ? specColor : mix(vec3(0.04), albedo, metallic);
    vec3 F = FresnelSchlick(hDotV, F0);
    float D = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(normal, V, lightDir, roughness);
    vec3 specular = vec3(0.0);
    if (nDotL > 1e-4 && nDotV > 1e-4) {
        specular = (D * G * F) / max(4.0 * nDotV * nDotL, 1e-4);
    }

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    const bool foliageCard = (drawData.litShadingModel & 16) != 0;
    float shadow = ComputeShadowFactor(normal, lightDir);
    if (alphaCutout) {
        shadow = max(shadow, foliageCard ? 0.94 : 0.88);
    }
    if (foliageCard) {
        shadow = 1.0;
    }
    float diffuseTerm = mix(max(nDotL, 0.14), nDotL, foliageCard ? 0.35 : 0.75);
    vec3 diffuse = (kD * albedo / kPi) * diffuseTerm;
    vec3 direct = (diffuse + (specular * max(nDotL, 0.0))) * lightColor * (foliageCard ? 1.05 : 1.30) * shadow;
    vec3 ambientBoost = cameraData.presentationExtra.yzw;
    vec3 ambient = albedo * (vec3(0.06, 0.065, 0.072) + ambientBoost * 0.14) * (1.0 - metallic * 0.45);
    if (tier >= 1.0) {
        float hemi = normal.y * 0.5 + 0.5;
        float roughStone = smoothstep(0.86, 0.98, roughness) * (1.0 - metallic);
        vec3 sky = mix(vec3(0.15, 0.18, 0.22), vec3(0.17, 0.17, 0.16), roughStone) + ambientBoost * 0.08;
        vec3 ground = vec3(0.05, 0.046, 0.043) + ambientBoost * 0.045;
        ambient += albedo * mix(ground, sky, hemi) * (foliageCard ? 0.10 : mix(0.075, 0.045, roughStone));
    }
    if (tier >= 2.0) {
        vec3 coatF0 = vec3(0.04);
        vec3 coatF = FresnelSchlick(hDotV, coatF0);
        float coatD = DistributionGGX(normal, H, 0.15);
        float coatG = GeometrySmith(normal, V, lightDir, 0.15);
        vec3 clearcoat = (coatD * coatG * coatF) / max(4.0 * nDotV * nDotL, 1e-4);
        direct += clearcoat * nDotL * 0.14;
    }
    vec3 pseudoReflection = ComputePseudoReflection(normal, viewDir, roughness, ambientBoost);
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), crystalStyle ? 3.0 : 5.0);
    float roughStone = smoothstep(0.86, 0.98, roughness) * (1.0 - metallic);
    float pseudoRtxStrength = layeredStyle ? 0.08 : (mixedMediaStyle ? 0.13 : (crystalStyle ? 0.28 : 0.05));
    pseudoRtxStrength *= mix(1.0, 0.18, roughStone);
    vec3 pseudoRtx = pseudoReflection * fresnel * pseudoRtxStrength;
    if (crystalStyle) {
        pseudoRtx += pseudoReflection * (0.10 + (1.0 - roughness) * 0.18);
        emissiveTexel += albedo * vec3(0.04, 0.10, 0.16);
    } else if (mixedMediaStyle) {
        pseudoRtx *= vec3(1.0, 0.97, 1.03);
    }
    if (retroStyle) {
        roughness = max(roughness, 0.82);
        pseudoRtx *= 0.12;
    }
    vec3 litRgb = (ambient * ao) + direct + drawData.emissiveColor + emissiveTexel + pseudoRtx;
    vec3 toLocal = cameraData.localLightPositionRange.xyz - worldPositionWs;
    float localDistance = length(toLocal);
    float localRange = max(cameraData.localLightPositionRange.w, 0.001);
    vec3 localDir = localDistance > 1e-4 ? (toLocal / localDistance) : vec3(0.0, 1.0, 0.0);
    float localNdotL = max(dot(normal, localDir), 0.0);
    float localAtten = clamp(1.0 - (localDistance / localRange), 0.0, 1.0);
    localAtten *= localAtten;
    vec3 localColor = cameraData.localLightColorIntensity.rgb * cameraData.localLightColorIntensity.w;
    litRgb += albedo * localColor * localNdotL * localAtten * (foliageCard ? 0.48 : 0.40);
    litRgb = max(litRgb, albedo * (foliageCard ? 0.06 : 0.012));
    if (mixedMediaStyle) {
        litRgb = mix(litRgb, ApplyRetroStyle(litRgb, postUv01, cameraData.postProcessSecondary.z), 0.18);
    }

    float fogAmount = clamp(1.0 - exp2(-viewDistanceWs * fogDensity), 0.0, 1.0);
    float horizonFactor = clamp(1.0 - max(normalize(viewDirectionWs).y, 0.0), 0.0, 1.0);
    vec3 fogColorNear = max(cameraData.sweetFxTonemapFogColorDefog.xyz, vec3(0.0));
    vec3 fogColorFar = fogColorNear * vec3(0.94, 1.02, 1.08);
    vec3 fogColor = mix(fogColorNear, fogColorFar, horizonFactor);
    vec3 color = mix(litRgb, fogColor, fogAmount * 0.18);
    vec3 linearLit = color * exposure;
    if (hybridHdrRadiance) {
        if (retroStyle) {
            linearLit = ApplyRetroStyle(linearLit, postUv01, cameraData.postProcessSecondary.z);
        }
        fragColor = vec4(linearLit, outputAlpha);
        return;
    }
    vec3 mapped = TonemapAcesApprox(linearLit);
    mapped = ApplyColorGrade(mapped, contrast, saturation);
    mapped = CalibrateRoughStoneColor(mapped, roughStone, layeredStyle, mixedMediaStyle);
    if (retroStyle) {
        mapped = ApplyRetroStyle(mapped, postUv01, cameraData.postProcessSecondary.z);
    }
    mapped = ApplySweetFxLumaCurve(mapped, curveAmt);
    mapped = ApplyPostProcessFx(mapped, postUv01);
    mapped = CalibrateRoughStoneColor(mapped, roughStone, layeredStyle, mixedMediaStyle);
    if (debandAmt > 1e-5) {
        float dl = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
        vec2 grad = vec2(dFdx(dl), dFdy(dl));
        float gradMag = clamp(length(grad) * 96.0, 0.0, 1.0);
        float rnd = Hash21(gl_FragCoord.xy + vec2(cameraData.postProcessSecondary.z * 13.7, cameraData.postProcessSecondary.z * 9.1)) - 0.5;
        mapped += rnd * debandAmt * (0.12 + gradMag * 0.55);
    }
    mapped = TriangularDitherRgb(mapped, postUv01, cameraData.postProcessSecondary.z, ditherAmt);
    fragColor = vec4(clamp(mapped, 0.0, 1.0), outputAlpha);
}
