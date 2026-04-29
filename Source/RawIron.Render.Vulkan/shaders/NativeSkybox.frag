#version 450

layout(location = 0) in vec3 vWorldRay;

layout(location = 0) out vec4 fragColor;

layout(std140, set = 0, binding = 0) uniform SkyUniforms {
    int hasSkyTexture;
    int _pad0;
    int _pad1;
    int _pad2;
    mat4 clipFromLocal;
    mat4 eyeToWorldRotation;
} sky;

layout(set = 1, binding = 0) uniform sampler2D skyEquirect;

vec2 equirectUv(vec3 dir) {
    vec3 d = normalize(dir);
    float phi = atan(d.z, d.x);
    float theta = asin(clamp(d.y, -1.0, 1.0));
    const float kPi = 3.14159265358979323846;
    return vec2(phi * (0.5 / kPi) + 0.5, theta / kPi + 0.5);
}

float hash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float noise3d(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);

    float n000 = hash31(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash31(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash31(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash31(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash31(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash31(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash31(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash31(i + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);
    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);
    return mix(nxy0, nxy1, u.z);
}

float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < 3; ++i) {
        value += noise3d(p * frequency) * amplitude;
        amplitude *= 0.5;
        frequency *= 2.03;
    }
    return value;
}

vec3 volumetricSky(vec3 rayDir, vec3 baseSky) {
    vec3 d = normalize(rayDir);
    vec3 cloudColor = vec3(0.88, 0.90, 0.94);
    vec3 sunTint = vec3(1.0, 0.95, 0.86);
    vec3 sky = baseSky;
    if (d.y <= -0.02) {
        return sky;
    }

    float transmittance = 1.0;
    vec3 accum = vec3(0.0);
    float t = 6.0;
    for (int step = 0; step < 10; ++step) {
        vec3 p = d * t;
        float layer = smoothstep(0.09, 0.76, d.y);
        float density = fbm(vec3(p.x * 0.0032, p.y * 0.0021 + 27.0, p.z * 0.0032));
        density = smoothstep(0.46, 0.76, density) * layer;
        float atten = exp(-density * 0.9);
        vec3 lit = mix(cloudColor * 0.82, cloudColor, density);
        lit += sunTint * (density * density) * 0.20;
        float weight = (1.0 - atten) * transmittance;
        accum += lit * weight;
        transmittance *= atten;
        if (transmittance < 0.025) {
            break;
        }
        t += 11.0;
    }

    return mix(accum + (sky * transmittance), sky, 0.16);
}

void main() {
    vec3 d = normalize(vWorldRay);
    vec3 zenith = vec3(0.18, 0.25, 0.36);
    vec3 horizon = vec3(0.64, 0.72, 0.84);
    float h = clamp(d.y * 0.55 + 0.45, 0.0, 1.0);
    vec3 baseGradient = mix(horizon, zenith, h);

    if (sky.hasSkyTexture != 0) {
        vec2 uv = equirectUv(vWorldRay);
        vec3 texSky = texture(skyEquirect, uv).rgb;
        float texLuma = dot(texSky, vec3(0.2126, 0.7152, 0.0722));
        float texWeight = 0.82 * smoothstep(0.03, 0.22, texLuma);
        vec3 blendedBase = mix(baseGradient, texSky, texWeight);
        fragColor = vec4(volumetricSky(d, blendedBase), 1.0);
    } else {
        fragColor = vec4(volumetricSky(d, baseGradient), 1.0);
    }
}
