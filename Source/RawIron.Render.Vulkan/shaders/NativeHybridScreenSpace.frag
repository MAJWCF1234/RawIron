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

float SampleDepth(vec2 uv01) {
    return texture(sceneDepth, uv01).r;
}

float Hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec2 uv = vec2(vUv.x, 1.0 - vUv.y);
    vec3 linearHdr = texture(hdrSceneLinear, uv).rgb;

    float centerDepth = SampleDepth(uv);
    vec2 texel = vec2(cameraData.viewportMetrics.z, cameraData.viewportMetrics.w);

    // Per-pixel rotation breaks fixed-step banding from a static kernel.
    float spin = Hash21(gl_FragCoord.xy + cameraData.viewportMetrics.xy) * 6.28318530718;

    float aoAccum = 0.0;
    float weightSum = 0.0;
    const int kKernel = 14;
    const float radiusInnerPx = 8.0;
    const float radiusOuterPx = 24.0;
    // Reject samples across large depth discontinuities (reduces halo on silhouettes / thin geometry).
    const float depthGuard = 0.002;

    for (int i = 0; i < kKernel; ++i) {
        float t = (float(i) + 0.37) / float(kKernel);
        float a = spin + t * 6.28318530718;
        float radiusPx = ((i & 1) != 0) ? radiusOuterPx : radiusInnerPx;
        vec2 offset = vec2(cos(a), sin(a)) * (radiusPx * texel);

        float neighborDepth = SampleDepth(uv + offset);
        float depthDelta = abs(neighborDepth - centerDepth);
        float similarDepth = 1.0 - smoothstep(depthGuard, depthGuard * 10.0, depthDelta);

        float delta = centerDepth - neighborDepth;
        float occ = 0.0;
        if (delta > 0.00003) {
            occ = smoothstep(0.00006, 0.032, delta) * similarDepth;
        }
        aoAccum += occ;
        weightSum += similarDepth;
    }

    float norm = max(weightSum, 0.35);
    float ao = clamp(1.0 - (aoAccum / norm) * 0.18, 0.35, 1.0);

    vec3 center = linearHdr;
    vec3 blurCross =
        texture(hdrSceneLinear, uv + vec2(texel.x * 2.0, 0.0)).rgb +
        texture(hdrSceneLinear, uv - vec2(texel.x * 2.0, 0.0)).rgb +
        texture(hdrSceneLinear, uv + vec2(0.0, texel.y * 2.0)).rgb +
        texture(hdrSceneLinear, uv - vec2(0.0, texel.y * 2.0)).rgb;
    blurCross *= 0.25;
    float glossyCue = clamp(1.0 - centerDepth * 1.8, 0.0, 1.0);
    float highlightMask = clamp(max(max(center.r, center.g), center.b) - 0.42, 0.0, 1.0);
    vec3 pseudoBounce = mix(center, blurCross, 0.55) * highlightMask * glossyCue * 0.08;

    fragColor = vec4((linearHdr * ao) + pseudoBounce, 1.0);
}
