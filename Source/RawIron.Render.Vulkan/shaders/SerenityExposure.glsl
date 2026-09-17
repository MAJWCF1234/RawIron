// Bliss deferred.vsh scene-linear meter, adapted from its atlas to native HDR mips.
// RG stores adapted photopic/rod luminance; BA stores exposure/rod response.
layout(set = 1, binding = 4) uniform sampler2D serenityExposureHistory;
vec2 SerenityR2(int n) {
    return fract(vec2(0.75487765, 0.56984026) * float(n));
}
vec4 SerenityMeterExposureRod() {
    float reduceLod = float(min(4, textureQueryLevels(hdrSceneLinear) - 1));
    int seed = int(serenity.temporalState.z) % 2000;
    vec2 logSum = vec2(0.0);
    for (int i = 0; i < 50; ++i) {
        vec2 tc = 0.5 + (SerenityR2(seed * 50 + i) - 0.5) * 0.7;
        vec3 sp = max(SerenityFinite(textureLod(hdrSceneLinear, tc, reduceLod).rgb), vec3(0.0));
        logSum += log(max(vec2(dot(sp, vec3(0.21, 0.72, 0.07)),
                              min(dot(sp, vec3(0.07, 0.22, 0.71)), 8e-2)), vec2(1e-8)));
    }
    vec2 brightness = exp(logSum / 50.0);
    // At 60 Hz/speed 1 these are Bliss's separate 0.95 / 0.985 retention factors.
    // Time scaling is a native adaptation; higher speed adapts faster.
    vec2 retention = pow(vec2(0.95, 0.985), vec2(serenity.temporalState.x * 60.0 * serenity.controls[8].x));
    if (serenity.temporalState.y > 0.5) {
        brightness = mix(brightness, texelFetch(serenityExposureHistory, ivec2(0), 0).rg, retention);
    }
    brightness = clamp(brightness, vec2(0.00003051757), vec2(65000.0));
    float exposure = max(0.18 / log2(brightness.x * 2.5 + 1.045) * 0.62, 0.0);
    float rod = max(0.012 / log2(brightness.y + 1.002) - 0.1, 0.0) * 1.2;
    return vec4(brightness, exposure, rod);
}
