// Native Serenity radiance stage. Runs on RawIron HDR + G-buffer, not Iris colortex.
// Bloom / bloomy fog / auto-exposure / Purkinje follow Bliss composite11 formulas.
// Screen-space bounce stands in for LPV until a voxel volume exists in the engine.

layout(set = 1, binding = 1) uniform sampler2D sceneDepth;
layout(set = 1, binding = 2) uniform sampler2D sceneNormalRoughness;
layout(set = 1, binding = 3) uniform sampler2D sceneMaterial;
#include "SerenityExposure.glsl"

int SerenityStageFlags() {
    return int(serenity.controls[6].w + 0.5);
}

float SerenityVignette(vec2 uv) {
    return clamp(1.5 - dot(uv - 0.5, uv - 0.5) * 2.0, 0.0, 1.5);
}

vec3 SerenityBloom(vec3 color, vec2 uv, float exposure) {
    vec2 texel = serenity.controls[6].xy;
    float threshold = max(serenity.controls[8].w, 0.0);
    float weights[5] = float[](1.0, 0.55, 0.28, 0.14, 0.07);
    float weightSum = 0.0;
    vec3 bloom = vec3(0.0);
    vec3 fogBloom = vec3(0.0);
    for (int lod = 0; lod < 5; ++lod) {
        float radius = 0.75 + float(lod) * 0.85;
        vec2 lodTexel = texel * exp2(float(lod));
        vec3 tap = vec3(0.0);
        vec3 fogTap = vec3(0.0);
        const vec2 corners[4] = vec2[](vec2(1,1), vec2(-1,1), vec2(1,-1), vec2(-1,-1));
        for (int corner = 0; corner < 4; ++corner) {
            vec2 sampleUv = clamp(uv + corners[corner] * radius * lodTexel, texel * 0.5, 1.0 - texel * 0.5);
            vec3 radiance = max(textureLod(hdrSceneLinear, sampleUv, float(lod)).rgb, vec3(0.0));
            tap += max(radiance - threshold, vec3(0.0));
            fogTap += radiance;
        }
        bloom += (tap * 0.25) * weights[lod];
        fogBloom += (fogTap * 0.25) * weights[lod];
        weightSum += weights[lod];
    }
    bloom = max(bloom / max(weightSum, 1e-4), vec3(0.0));
    fogBloom /= max(weightSum, 1e-4);
    float strength = max(serenity.controls[7].x, 0.0);
    float lightScat = clamp(strength * 0.5 * pow(max(exposure, 0.0), 0.2), 0.0, 1.0) * SerenityVignette(uv);
    float depth = texture(sceneDepth, uv).r;
    float vl = 0.0;
    if ((SerenityStageFlags() & 16) != 0) {
        vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
        vec4 world = serenity.invViewProjection * vec4(ndc, depth, 1.0);
        float dist = length(world.xyz / max(abs(world.w), 1e-6) - serenity.cameraWorldPosition.xyz);
        float fogAmount = (1.0 - exp(-dist * 0.014 * max(serenity.controls[7].y, 0.0)))
            * clamp(1.0 - pow(max(abs(uv.x - 0.5), abs(uv.y - 0.5)) * 2.0, 15.0), 0.0, 1.0);
        vl = clamp(fogAmount, 0.0, 1.0);
    }
    // Bliss fog scatters full radiance. A highlight threshold must never turn
    // a distant sky/surface black simply because it is below BLOOM_THRESHOLD.
    return SerenityFinite((mix(color, fogBloom, vl) + bloom * lightScat));
}

vec3 SerenityPurkinje(vec3 color, float rodExposure, float exposure) {
    if ((SerenityStageFlags() & 8) == 0) {
        return color;
    }
    float strength = clamp(serenity.controls[7].z, 0.0, 1.0);
    float lum = dot(color, vec3(0.15, 0.3, 0.55));
    float lum2 = dot(color, vec3(0.85, 0.7, 0.45));
    float purkinje = clamp(rodExposure / (1.0 + rodExposure) * strength, 0.0, 1.0);
    if ((SerenityStageFlags() & 1) != 0) {
        purkinje *= clamp(exposure * exposure, 0.0, 1.0);
    }
    float rodLum = lum2 * 200.0;
    float rodCurve = clamp(mix(1.0, rodLum / (2.5 + rodLum), clamp(purkinje, 0.0, 1.0)), 0.0, 1.0);
    vec3 night = lum * serenity.purkinjeColor.rgb * serenity.purkinjeColor.a;
    return mix(night, color, rodCurve);
}

vec3 SerenityWorldFromDepth(vec2 uv, float depth) {
    vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    vec4 world = serenity.invViewProjection * vec4(ndc, depth, 1.0);
    return world.xyz / max(abs(world.w), 1e-8);
}

vec3 SerenityScreenBounce(vec3 color, vec2 uv) {
    // Screen-space irradiance using reconstructed world positions. Native home for
    // Bliss LPV until a voxel volume is allocated in the scene renderer.
    vec3 encoded = texture(sceneNormalRoughness, uv).xyz;
    vec3 normal = normalize(encoded * 2.0 - 1.0);
    float depth = texture(sceneDepth, uv).r;
    if (depth > 0.9995) {
        return color;
    }
    vec3 world = SerenityWorldFromDepth(uv, depth);
    vec2 texel = serenity.controls[6].xy;
    vec2 dirs[8] = vec2[](
        vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0),
        vec2(0.707, 0.707), vec2(-0.707, 0.707), vec2(0.707, -0.707), vec2(-0.707, -0.707));
    vec3 bounce = vec3(0.0);
    float weight = 0.0;
    for (int ring = 0; ring < 2; ++ring) {
        float radius = mix(14.0, 32.0, float(ring));
        for (int i = 0; i < 8; ++i) {
            vec2 tapUv = clamp(uv + dirs[i] * texel * radius, vec2(0.0), vec2(1.0));
            float tapDepth = texture(sceneDepth, tapUv).r;
            if (tapDepth > 0.9995) {
                continue;
            }
            vec3 tapWorld = SerenityWorldFromDepth(tapUv, tapDepth);
            vec3 toTap = tapWorld - world;
            float dist2 = dot(toTap, toTap);
            if (dist2 < 1e-5) {
                continue;
            }
            float ndotl = max(dot(normal, toTap * inversesqrt(dist2)), 0.0);
            float vis = ndotl / (1.0 + dist2 * 0.12);
            vis *= (1.0 - smoothstep(0.0, 0.09, abs(tapDepth - depth)));
            bounce += max(texture(hdrSceneLinear, tapUv).rgb, vec3(0.0)) * vis;
            weight += vis;
        }
    }
    if (weight < 1e-4) {
        return color;
    }
    return color + (bounce / weight) * 0.16 * (1.0 - encoded.z);
}

vec3 SerenityWaterCaustics(vec3 color, vec2 uv) {
    vec4 material = texture(sceneMaterial, uv);
    int flags = int(material.w);
    if ((flags & 8) == 0) {
        return color;
    }
    float t = serenity.controls[6].z;
    float caustic = pow(0.5 + 0.5 * sin(uv.x * 42.0 + t * 1.7) * sin(uv.y * 37.0 - t * 1.3), 6.0);
    return color + vec3(0.18, 0.32, 0.38) * caustic * 0.45;
}

vec3 SerenityWaterRefract(vec3 color, vec2 uv) {
    float amount = serenity.controls[8].z;
    vec4 material = texture(sceneMaterial, uv);
    if (amount < 1e-4 || (int(material.w) & 8) == 0) {
        return color;
    }
    vec3 normal = texture(sceneNormalRoughness, uv).xyz * 2.0 - 1.0;
    vec2 offset = normal.xz * (0.028 * amount);
    vec2 refractUv = clamp(uv + offset, vec2(0.002), vec2(0.998));
    float behindDepth = texture(sceneDepth, refractUv).r;
    float waterDepth = texture(sceneDepth, uv).r;
    vec3 waterWorld = SerenityWorldFromDepth(uv, waterDepth);
    vec3 behindWorld = SerenityWorldFromDepth(refractUv, behindDepth);
    float thickness = max(length(behindWorld - waterWorld), 0.0);
    vec3 absorb = exp(-vec3(0.55, 0.16, 0.10) * thickness * (0.35 + amount * 0.4));
    vec3 behind = max(texture(hdrSceneLinear, refractUv).rgb, vec3(0.0)) * absorb;
    if (behindDepth <= waterDepth + 0.0005) {
        return color * mix(vec3(1.0), absorb, 0.35);
    }
    return mix(behind, color, 0.38);
}

vec3 SerenityVolumetricLight(vec3 color, vec2 uv) {
    if ((SerenityStageFlags() & 16) == 0) {
        return color;
    }
    vec3 sun = serenity.lightDirectionIntensity.xyz;
    float sunStrength = max(serenity.lightDirectionIntensity.w, 0.0);
    if (sunStrength < 1e-4 || dot(sun, sun) < 1e-8) {
        return color;
    }
    sun = normalize(sun);
    vec4 sunClip = serenity.viewProjection * vec4(serenity.cameraWorldPosition.xyz + sun * 800.0, 1.0);
    if (sunClip.w <= 1e-4) {
        return color;
    }
    vec2 sunNdc = sunClip.xy / sunClip.w;
    vec2 sunUv = vec2(sunNdc.x * 0.5 + 0.5, 0.5 - sunNdc.y * 0.5);
    float onScreen = 1.0 - smoothstep(0.62, 1.25, length(sunUv - vec2(0.5)));
    if (onScreen < 1e-3) {
        return color;
    }
    vec2 texel = serenity.controls[6].xy;
    vec2 stepUv = (sunUv - uv) / 12.0;
    if (dot(stepUv, stepUv) < dot(texel, texel)) {
        return color;
    }
    vec3 sunTint = normalize(max(serenity.directionalLightColorIntensity.rgb, vec3(0.05)));
    vec3 accum = vec3(0.0);
    float weight = 1.0;
    vec2 coord = uv;
    for (int i = 0; i < 12; ++i) {
        coord += stepUv;
        if (coord.x < 0.0 || coord.x > 1.0 || coord.y < 0.0 || coord.y > 1.0) {
            break;
        }
        float tapDepth = texture(sceneDepth, coord).r;
        float sky = smoothstep(0.96, 0.999, tapDepth);
        vec3 radiance = max(texture(hdrSceneLinear, coord).rgb, vec3(0.0));
        accum += mix(vec3(0.0), radiance + sunTint * 0.35, sky) * weight;
        weight *= 0.84;
    }
    float amount = clamp(serenity.controls[7].y * 0.045 * onScreen * sunStrength, 0.0, 0.85);
    return color + SerenityFinite(accum * amount);
}

vec3 SerenityPrepareRadiance(vec3 color, vec2 uv, float exposure, float rodExposure) {
    color = max(color, vec3(0.0));
    color = SerenityScreenBounce(color, uv);
    color = SerenityWaterRefract(color, uv);
    color = SerenityWaterCaustics(color, uv);
    color = SerenityBloom(color, uv, exposure);
    color = SerenityVolumetricLight(color, uv);
    color *= exposure;
    color = SerenityPurkinje(color, rodExposure, exposure);
    return SerenityFinite(color);
}
