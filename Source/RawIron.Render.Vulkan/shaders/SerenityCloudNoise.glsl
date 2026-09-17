// Shared cloud noise for NativeSkybox volumetrics and NativeScenePreview cloud shadows.
float SerenityHash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float SerenityNoise3d(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    float n000 = SerenityHash31(i + vec3(0.0, 0.0, 0.0));
    float n100 = SerenityHash31(i + vec3(1.0, 0.0, 0.0));
    float n010 = SerenityHash31(i + vec3(0.0, 1.0, 0.0));
    float n110 = SerenityHash31(i + vec3(1.0, 1.0, 0.0));
    float n001 = SerenityHash31(i + vec3(0.0, 0.0, 1.0));
    float n101 = SerenityHash31(i + vec3(1.0, 0.0, 1.0));
    float n011 = SerenityHash31(i + vec3(0.0, 1.0, 1.0));
    float n111 = SerenityHash31(i + vec3(1.0, 1.0, 1.0));
    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);
    return mix(mix(nx00, nx10, u.y), mix(nx01, nx11, u.y), u.z);
}

float SerenityCloudFbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < 3; ++i) {
        value += SerenityNoise3d(p * frequency) * amplitude;
        amplitude *= 0.5;
        frequency *= 2.03;
    }
    return value;
}

float SerenityCloudDensity(vec3 worldPos, float time) {
    vec3 p = vec3(worldPos.x * 0.0032 + time * 0.018,
                  worldPos.y * 0.0021 + 27.0,
                  worldPos.z * 0.0032 + time * 0.011);
    return smoothstep(0.52, 0.84, SerenityCloudFbm(p));
}

float SerenityCloudDensity(vec3 worldPos) {
    return SerenityCloudDensity(worldPos, 0.0);
}
