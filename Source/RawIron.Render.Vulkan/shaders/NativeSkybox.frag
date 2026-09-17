#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 vWorldRay;

layout(location = 0) out vec4 fragColor;

layout(std140, set = 0, binding = 0) uniform SkyUniforms {
    int hasSkyTexture;
    int useAuthoredGradient;
    int volumetricClouds;
    int _pad1;
    mat4 clipFromLocal;
    mat4 eyeToWorldRotation;
    vec4 sunDirection;
    vec4 sunColor;
    vec4 horizonColor;
    vec4 zenithColor;
} sky;

layout(set = 1, binding = 0) uniform sampler2D skyEquirect;
#include "SerenityCloudNoise.glsl"

vec2 equirectUv(vec3 dir) {
    vec3 d = normalize(dir);
    float phi = atan(d.z, d.x);
    float theta = asin(clamp(d.y, -1.0, 1.0));
    const float kPi = 3.14159265358979323846;
    return vec2(phi * (0.5 / kPi) + 0.5, theta / kPi + 0.5);
}

vec3 applySunDisc(vec3 rayDir, vec3 baseSky) {
    vec3 sunDir = sky.sunDirection.xyz;
    float sunStrength = max(sky.sunDirection.w, 0.0);
    if (sunStrength <= 1e-4 || dot(sunDir, sunDir) <= 1e-4) {
        return baseSky;
    }
    sunDir = normalize(sunDir);
    vec3 d = normalize(rayDir);
    float alignment = dot(d, sunDir);
    float disc = smoothstep(0.9982, 0.99985, alignment);
    float glow = pow(max(alignment, 0.0), 96.0) * sunStrength;
    float skyWarmth = pow(max(alignment, 0.0), 8.0) * 0.42 * sunStrength;
    vec3 sunTint = normalize(max(sky.sunColor.rgb, vec3(0.05)));
    return baseSky + sunTint * (disc * 3.2 + glow * 0.65) + sunTint * 0.18 * skyWarmth;
}

vec3 volumetricSky(vec3 rayDir, vec3 baseSky) {
    vec3 d = normalize(rayDir);
    vec3 cloudColor = vec3(0.88, 0.90, 0.94);
    vec3 sunTint = normalize(max(sky.sunColor.rgb, vec3(0.05)));
    vec3 skyOut = baseSky;
    if (d.y <= -0.02) {
        return skyOut;
    }
    vec3 sunDir = sky.sunDirection.xyz;
    float sunStrength = max(sky.sunDirection.w, 0.0);
    float sunFacing = 0.0;
    if (sunStrength > 1e-4 && dot(sunDir, sunDir) > 1e-4) {
        sunFacing = pow(max(dot(d, normalize(sunDir)), 0.0), 14.0) * sunStrength;
    }

    float transmittance = 1.0;
    vec3 accum = vec3(0.0);
    float t = 6.0;
    for (int step = 0; step < 10; ++step) {
        vec3 p = d * t;
        float layer = smoothstep(0.09, 0.76, d.y);
        float density = SerenityCloudDensity(p, sky.sunColor.w) * layer * 0.65;
        float atten = exp(-density * 0.9);
        vec3 lit = mix(cloudColor * 0.82, cloudColor, density);
        lit += sunTint * (density * density) * (0.20 + sunFacing * 0.42);
        float weight = (1.0 - atten) * transmittance;
        accum += lit * weight;
        transmittance *= atten;
        if (transmittance < 0.025) {
            break;
        }
        t += 11.0;
    }

    return mix(accum + (skyOut * transmittance), skyOut, 0.16);
}

void main() {
    vec3 d = normalize(vWorldRay);
    vec3 sunDir = normalize(sky.sunDirection.xyz);
    float sunAlign = dot(d, sunDir);
    vec3 zenith = max(sky.zenithColor.rgb, vec3(0.02));
    vec3 horizon = max(sky.horizonColor.rgb, vec3(0.02));
    if (sky.useAuthoredGradient == 0) {
        zenith = vec3(0.54, 0.56, 0.57);
        horizon = vec3(0.82, 0.82, 0.80);
    }
    float h = clamp(d.y * 0.55 + 0.45, 0.0, 1.0);
    vec3 baseGradient = mix(horizon, zenith, h);
    baseGradient += normalize(max(sky.sunColor.rgb, vec3(0.05))) * 0.12
        * pow(max(sunAlign, 0.0), 5.0) * max(sky.sunDirection.w, 0.0);

    if (sky.hasSkyTexture != 0) {
        vec2 uv = equirectUv(vWorldRay);
        vec3 texSky = texture(skyEquirect, uv).rgb;
        float texLuma = dot(texSky, vec3(0.2126, 0.7152, 0.0722));
        vec3 foggyTexture = mix(vec3(texLuma), texSky, 0.18);
        foggyTexture = mix(foggyTexture, horizon, 0.42);
        foggyTexture = max(foggyTexture, vec3(0.56, 0.57, 0.56));
        float texWeight = 0.38 * smoothstep(0.03, 0.22, texLuma);
        vec3 blendedBase = mix(baseGradient, foggyTexture, texWeight);
        blendedBase = applySunDisc(d, blendedBase);
        if (sky.volumetricClouds != 0) {
            blendedBase = mix(blendedBase, volumetricSky(d, blendedBase), 0.42);
        }
        fragColor = vec4(blendedBase, 1.0);
    } else {
        vec3 withSun = applySunDisc(d, baseGradient);
        if (sky.volumetricClouds != 0) {
            withSun = mix(withSun, volumetricSky(d, withSun), 0.38);
        }
        fragColor = vec4(withSun, 1.0);
    }
}
