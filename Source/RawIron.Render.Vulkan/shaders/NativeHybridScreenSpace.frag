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

layout(set = 1, binding = 0) uniform sampler2D hdrSceneLinear;
layout(set = 1, binding = 1) uniform sampler2D sceneDepth;
layout(set = 1, binding = 2) uniform sampler2D sceneNormalRoughness;
layout(set = 1, binding = 3) uniform sampler2D sceneMaterial;

float SampleDepth(vec2 uv01) {
    return texture(sceneDepth, uv01).r;
}

float Hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec3 TraceScreenSpaceReflection(vec2 uv,
                                vec3 centerNormal,
                                float roughness,
                                float metallic,
                                float centerDepth,
                                vec2 texel,
                                float qualityTier,
                                out float hitConfidence) {
    hitConfidence = 0.0;
    float gloss = (1.0 - roughness) * mix(0.35, 1.0, metallic * 0.85 + 0.15);
    if (gloss < 0.03) {
        return vec3(0.0);
    }

    vec2 screenPos = uv * 2.0 - 1.0;
    vec3 viewWs = normalize(vec3(screenPos.x * 0.62, screenPos.y * 0.62 + 0.38, 1.0));
    vec3 reflectDir = reflect(-viewWs, centerNormal);
    vec2 marchDir = reflectDir.xy;
    float marchLen = length(marchDir);
    if (marchLen < 1e-5) {
        marchDir = normalize(centerNormal.xy + vec2(1e-4));
    } else {
        marchDir /= marchLen;
    }
    marchDir *= texel * mix(5.0, 24.0, gloss);

    int maxSteps = int(mix(5.0, 12.0, qualityTier * 0.5 + gloss * 0.5));
    vec3 accum = vec3(0.0);
    float weightSum = 0.0;
    float thickness = mix(0.0010, 0.038, gloss);

    for (int i = 1; i <= 12; ++i) {
        if (i > maxSteps) {
            break;
        }
        float t = float(i) / float(maxSteps);
        float jitter = Hash21(uv + vec2(float(i) * 17.3, float(i) * 9.1)) * 0.28;
        float marchT = 1.0 - pow(1.0 - t, mix(1.35, 1.85, roughness));
        vec2 suv = clamp(uv + marchDir * (marchT + jitter * 0.08), texel * 0.5, vec2(1.0) - texel * 0.5);
        float sampleDepth = SampleDepth(suv);
        float depthDiff = centerDepth - sampleDepth;
        if (depthDiff > 0.00035 && depthDiff < thickness * (0.45 + t * 1.6)) {
            vec3 hitNormal = normalize(texture(sceneNormalRoughness, suv).xyz * 2.0 - 1.0);
            float normalSimilarity = smoothstep(0.12, 0.78, dot(centerNormal, hitNormal));
            vec3 hitColor = texture(hdrSceneLinear, suv).rgb;
            float w = gloss * (1.0 - t * 0.68) * normalSimilarity * smoothstep(0.00035, 0.0016, depthDiff);
            accum += hitColor * w;
            weightSum += w;
            if (weightSum > 0.9) {
                break;
            }
        }
    }

    if (weightSum < 1e-4) {
        return vec3(0.0);
    }
    hitConfidence = clamp(weightSum, 0.0, 1.0);
    return accum / weightSum;
}

vec3 EnvironmentReflectionFallback(vec3 centerNormal,
                                   vec3 viewWs,
                                   float roughness,
                                   float glossyCue,
                                   vec3 fogNear,
                                   vec3 ambientBoost) {
    vec3 reflectDir = reflect(-viewWs, centerNormal);
    float horizon = clamp(reflectDir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky = mix(fogNear * 0.48, fogNear + ambientBoost * 0.35 + vec3(0.05, 0.06, 0.07), horizon);
    vec3 ground = fogNear * 0.34 + ambientBoost * 0.12;
    vec3 env = mix(ground, sky, horizon);
    float fresnel = pow(1.0 - max(dot(centerNormal, viewWs), 0.0), mix(3.0, 6.0, roughness));
    return env * glossyCue * fresnel * (0.22 + (1.0 - roughness) * 0.42);
}

void main() {
    vec2 uv = vec2(vUv.x, 1.0 - vUv.y);
    vec3 linearHdr = texture(hdrSceneLinear, uv).rgb;
    vec4 normalRoughness = texture(sceneNormalRoughness, uv);
    vec3 centerNormal = normalize(normalRoughness.xyz * 2.0 - 1.0);
    float roughness = clamp(normalRoughness.a, 0.04, 1.0);
    vec4 material = texture(sceneMaterial, uv);
    float metallic = clamp(material.r, 0.0, 1.0);
    float materialAo = clamp(material.g, 0.2, 1.0);
    float emissiveMask = clamp(material.b, 0.0, 1.0);
    float packedMaterialFlags = material.a;
    float materialFlags = floor(packedMaterialFlags + 0.001);
    float sunShadow = clamp((packedMaterialFlags - materialFlags) * 100.0, 0.0, 1.0);
    if (sunShadow <= 1e-4) {
        sunShadow = 1.0;
    }

    float centerDepth = SampleDepth(uv);
    if (centerDepth > 0.9995) {
        fragColor = vec4(linearHdr, 1.0);
        return;
    }
    vec2 texel = vec2(cameraData.viewportMetrics.z, cameraData.viewportMetrics.w);
    float qualityTier = clamp(cameraData.sweetFxTonemapStrengthPad.y, 0.0, 2.0);

    // Per-pixel rotation breaks fixed-step banding from a static kernel.
    float spin = Hash21(gl_FragCoord.xy + cameraData.viewportMetrics.xy) * 6.28318530718;

    float aoAccum = 0.0;
    float weightSum = 0.0;
    int aoKernel = int(mix(8.0, 12.0, qualityTier * 0.5 + 0.5));
    bool skipAo = metallic > 0.94 && roughness < 0.22;
    const int kKernel = 12;
    const float goldenAngle = 2.39996323;
    const float radiusInnerPx = 7.0;
    const float radiusOuterPx = 22.0;
    // Reject samples across large depth discontinuities (reduces halo on silhouettes / thin geometry).
    const float depthGuard = 0.002;

    for (int i = 0; i < kKernel; ++i) {
        if (skipAo || i >= aoKernel) {
            break;
        }
        float tap = float(i) + 0.5;
        float ringMix = tap / float(aoKernel);
        float radiusPx = mix(radiusInnerPx, radiusOuterPx, ringMix);
        float a = spin + tap * goldenAngle;
        vec2 offset = vec2(cos(a), sin(a)) * (radiusPx * texel);

        vec2 sampleUv = clamp(uv + offset, texel * 0.5, vec2(1.0) - texel * 0.5);
        float neighborDepth = SampleDepth(sampleUv);
        vec3 sampleNormal = normalize(texture(sceneNormalRoughness, sampleUv).xyz * 2.0 - 1.0);
        float depthDelta = abs(neighborDepth - centerDepth);
        float similarDepth = 1.0 - smoothstep(depthGuard, depthGuard * 10.0, depthDelta);
        float similarNormal = smoothstep(0.08, 0.65, dot(centerNormal, sampleNormal));

        float delta = centerDepth - neighborDepth;
        float occ = 0.0;
        if (delta > 0.00003) {
            occ = smoothstep(0.00006, 0.032, delta) * similarDepth * similarNormal;
        }
        aoAccum += occ;
        weightSum += similarDepth * similarNormal;
    }

    float norm = max(weightSum, 0.35);
    float aoStrength = mix(0.28, 0.40, qualityTier * 0.5);
    float ao = mix(1.0, clamp(1.0 - (aoAccum / norm), 0.58, 1.0), aoStrength) * materialAo;

    vec3 center = linearHdr;
    vec2 blurOffsets[4] = vec2[4](
        vec2(texel.x * 2.0, 0.0),
        vec2(-texel.x * 2.0, 0.0),
        vec2(0.0, texel.y * 2.0),
        vec2(0.0, -texel.y * 2.0));
    vec3 blurCross = vec3(0.0);
    float blurWeightSum = 0.0;
    for (int bi = 0; bi < 4; ++bi) {
        vec2 buv = clamp(uv + blurOffsets[bi], texel * 0.5, vec2(1.0) - texel * 0.5);
        float bDepth = SampleDepth(buv);
        float depthW = 1.0 - smoothstep(0.0008, 0.006, abs(bDepth - centerDepth));
        blurCross += texture(hdrSceneLinear, buv).rgb * depthW;
        blurWeightSum += depthW;
    }
    blurCross /= max(blurWeightSum, 1e-4);
    float glossyCue = clamp((1.0 - roughness) * (0.25 + metallic * 0.75), 0.0, 1.0);
    float highlightMask = clamp(max(max(center.r, center.g), center.b) - 0.38, 0.0, 1.0);
    highlightMask = min(highlightMask, 0.55);
    float hybridGlow = materialFlags >= 2.0 ? 0.10 : 0.0;
    float diffuseBounceStrength = (1.0 - metallic) * (1.0 - roughness * 0.35);
    vec3 pseudoBounce = mix(center, blurCross, 0.58) * highlightMask * diffuseBounceStrength * (0.042 + hybridGlow);
    vec3 pseudoReflection = blurCross * glossyCue * highlightMask * mix(0.05, 0.09, qualityTier * 0.5);
    float emissiveLuma = clamp(max(max(center.r, center.g), center.b) - 0.25, 0.0, 1.0);
    vec3 emissiveBleed = blurCross * emissiveMask * 0.08 + center * emissiveMask * emissiveLuma * 0.14;
    if (qualityTier >= 1.0 && emissiveMask > 0.06) {
        vec2 wideOffset = vec2(texel.x, texel.y) * mix(5.0, 7.0, qualityTier * 0.5);
        vec2 guvP = clamp(uv + wideOffset, texel * 0.5, vec2(1.0) - texel * 0.5);
        vec2 guvN = clamp(uv - wideOffset, texel * 0.5, vec2(1.0) - texel * 0.5);
        float gWp = 1.0 - smoothstep(0.0012, 0.012, abs(SampleDepth(guvP) - centerDepth));
        float gWn = 1.0 - smoothstep(0.0012, 0.012, abs(SampleDepth(guvN) - centerDepth));
        vec3 wideGlow = texture(hdrSceneLinear, guvP).rgb * gWp + texture(hdrSceneLinear, guvN).rgb * gWn;
        wideGlow /= max(gWp + gWn, 1e-4);
        emissiveBleed += wideGlow * emissiveMask * mix(0.05, 0.11, qualityTier * 0.5);
    }
    float ceilingFacing = smoothstep(-0.98, -0.55, centerNormal.y);
    vec3 ceilingBounce = blurCross * ceilingFacing * (1.0 - metallic) * mix(0.03, 0.07, qualityTier * 0.5);
    vec2 screenPos = uv * 2.0 - 1.0;
    vec3 viewWs = normalize(vec3(screenPos.x * 0.62, screenPos.y * 0.62 + 0.38, 1.0));
    float ssrHitConfidence = 0.0;
    vec3 screenSpaceReflection = vec3(0.0);
    if (glossyCue > 0.04) {
        screenSpaceReflection = TraceScreenSpaceReflection(
            uv, centerNormal, roughness, metallic, centerDepth, texel, qualityTier, ssrHitConfidence);
    }
    float ssrStrength = mix(0.05, 0.24, qualityTier * 0.5) * glossyCue * (0.55 + metallic * 0.35);
    // Near fog tint only — do not pre-mix fog_far here (distance fog handles far tint in the forward pass).
    vec3 envTint = max(cameraData.sweetFxTonemapFogColorDefog.xyz, vec3(0.02));
    vec3 ambientBoost = cameraData.presentationExtra.yzw;
    vec3 envFallback = EnvironmentReflectionFallback(
        centerNormal, viewWs, roughness, glossyCue, envTint, ambientBoost);
    float fallbackWeight = (1.0 - ssrHitConfidence) * mix(0.18, 0.42, qualityTier * 0.5);
    vec3 reflectionRadiance =
        screenSpaceReflection * ssrStrength + envFallback * fallbackWeight + pseudoReflection;
    float groundContact = smoothstep(0.74, 0.98, centerNormal.y) * (1.0 - metallic);
    float contactDarken = 1.0 - groundContact * mix(0.04, 0.10, qualityTier * 0.5) * (1.0 - ao);
    float shadowPreserve = sunShadow;
    pseudoBounce *= shadowPreserve;
    reflectionRadiance *= mix(0.40, 1.0, shadowPreserve);
    ceilingBounce *= shadowPreserve;
    emissiveBleed *= mix(0.72, 1.0, shadowPreserve);

    float aoDarken = clamp(ao * contactDarken, 0.62, 1.0);
    vec3 baseRadiance = center * aoDarken;
    vec3 addedRadiance = pseudoBounce + reflectionRadiance + emissiveBleed + ceilingBounce;
    float centerLuma = dot(center, vec3(0.2126, 0.7152, 0.0722));
    float addedCap = max(centerLuma * 1.8, 0.12);
    float addedLuma = dot(addedRadiance, vec3(0.2126, 0.7152, 0.0722));
    if (addedLuma > addedCap) {
        addedRadiance *= (addedCap / max(addedLuma, 1e-4));
    }
    fragColor = vec4(baseRadiance + addedRadiance, 1.0);
}
