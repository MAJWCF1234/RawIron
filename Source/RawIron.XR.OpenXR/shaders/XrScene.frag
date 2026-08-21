#version 450

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) flat in vec4 inAtlasRect;
layout(location = 5) flat in vec4 inNormalAtlasRect;
layout(location = 6) flat in vec4 inMaterialParams;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneAtlas;

layout(push_constant) uniform EyeData {
    mat4 viewProjection;
    vec4 cameraWorldPosition;
} eye;

bool hasAtlasRect(vec4 rect) {
    return rect.z > rect.x && rect.w > rect.y;
}

vec3 safeNormal(vec3 value, vec3 fallback) {
    float lengthSquared = dot(value, value);
    return lengthSquared > 1.0e-7 ? value * inversesqrt(lengthSquared) : fallback;
}

vec3 sampleTangentNormal(vec3 geometricNormal) {
    if (!hasAtlasRect(inNormalAtlasRect)) return geometricNormal;
    vec2 atlasUv = mix(inNormalAtlasRect.xy, inNormalAtlasRect.zw, fract(inTexCoord));
    vec3 sampled = texture(sceneAtlas, atlasUv).xyz * 2.0 - 1.0;
    sampled.xy *= inMaterialParams.zw;
    sampled = safeNormal(sampled, vec3(0.0, 0.0, 1.0));

    // The shared Raw Iron mesh stream does not require precomputed tangents. Derivatives build a
    // stable local frame for any imported mesh with UVs, including the three.js normal-map assets.
    vec3 dp1 = dFdx(inWorldPosition);
    vec3 dp2 = dFdy(inWorldPosition);
    vec2 duv1 = dFdx(inTexCoord);
    vec2 duv2 = dFdy(inTexCoord);
    float determinant = duv1.x * duv2.y - duv1.y * duv2.x;
    if (abs(determinant) < 1.0e-7) return geometricNormal;
    vec3 tangent = dp1 * duv2.y - dp2 * duv1.y;
    tangent = tangent - geometricNormal * dot(geometricNormal, tangent);
    tangent = safeNormal(tangent, vec3(1.0, 0.0, 0.0));
    vec3 bitangent = safeNormal(cross(geometricNormal, tangent), vec3(0.0, 0.0, 1.0));
    return safeNormal(tangent * sampled.x + bitangent * sampled.y + geometricNormal * sampled.z, geometricNormal);
}

void main() {
    vec3 albedo = vec3(1.0);
    if (hasAtlasRect(inAtlasRect)) {
        vec2 atlasUv = mix(inAtlasRect.xy, inAtlasRect.zw, fract(inTexCoord));
        albedo = texture(sceneAtlas, atlasUv).rgb;
    }
    albedo *= inColor;

    vec3 normal = safeNormal(inNormal, vec3(0.0, 1.0, 0.0));
    normal = sampleTangentNormal(normal);
    vec3 viewDirection = safeNormal(eye.cameraWorldPosition.xyz - inWorldPosition, vec3(0.0, 0.0, 1.0));
    vec3 lightDirection = safeNormal(vec3(-0.35, 0.82, -0.45), vec3(0.0, 1.0, 0.0));
    vec3 halfDirection = safeNormal(lightDirection + viewDirection, lightDirection);
    float nDotL = max(dot(normal, lightDirection), 0.0);
    float nDotV = max(dot(normal, viewDirection), 0.0);
    float nDotH = max(dot(normal, halfDirection), 0.0);
    float metallic = clamp(inMaterialParams.x, 0.0, 1.0);
    float roughness = clamp(inMaterialParams.y, 0.045, 1.0);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    float fresnelPower = pow(1.0 - max(dot(halfDirection, viewDirection), 0.0), 5.0);
    vec3 fresnel = f0 + (vec3(1.0) - f0) * fresnelPower;
    float specularExponent = mix(256.0, 4.0, roughness * roughness);
    vec3 specular = fresnel * pow(nDotH, specularExponent) * (1.0 - roughness * 0.45);
    vec3 diffuse = albedo * (1.0 - metallic) * (0.16 + nDotL * 0.84);
    float skyFacing = normal.y * 0.5 + 0.5;
    vec3 skyAmbient = mix(vec3(0.045, 0.055, 0.075), vec3(0.20, 0.27, 0.36), skyFacing);
    vec3 lit = diffuse * (skyAmbient + vec3(1.0, 0.92, 0.80) * nDotL * 1.18)
        + specular * (0.18 + nDotL * 1.35);
    lit += albedo * (0.018 + 0.032 * nDotV);
    // Simple filmic shoulder keeps head-mounted highlights comfortable rather than hard-clipping.
    lit = lit / (lit + vec3(1.0));
    outColor = vec4(pow(max(lit, vec3(0.0)), vec3(1.0 / 2.2)), 1.0);
}
