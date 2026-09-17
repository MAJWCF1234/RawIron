// Native temporal resolve matching Bliss composite5.fsh (closest-velocity TAA).
// History is the previous swapchain copy. Velocity comes from engine depth plus
// previous/inverse view-projection; Iris entity velocity attributes are not used.
layout(set = 1, binding = 1) uniform sampler2D sceneDepth;
layout(set = 1, binding = 2) uniform sampler2D serenityHistory;

vec3 SerenityReprojectHistoryUv(vec2 uv, float depth) {
    // Negative viewport height maps NDC +Y to the top of the image (uv.y = 0).
    vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    vec4 world = serenity.invViewProjection * vec4(ndc, depth, 1.0);
    if (abs(world.w) < 1e-8) {
        return vec3(uv, 0.0);
    }
    world.xyz /= world.w;
    vec4 previous = serenity.previousViewProjection * vec4(world.xyz, 1.0);
    if (previous.w <= 1e-8) {
        return vec3(uv, 0.0);
    }
    vec2 previousNdc = previous.xy / previous.w;
    vec2 historyUv = vec2(previousNdc.x * 0.5 + 0.5, 0.5 - previousNdc.y * 0.5);
    float valid = float(historyUv.x >= 0.0 && historyUv.x <= 1.0 && historyUv.y >= 0.0 && historyUv.y <= 1.0);
    return vec3(historyUv, valid);
}

// Bliss closestToCamera5taps: nearest depth in a 2-pixel 5-tap cross of corners.
vec3 SerenityClosestToCamera(vec2 uv, vec2 texel) {
    texel *= 2.0;
    vec2 du = vec2(texel.x, 0.0);
    vec2 dv = vec2(0.0, texel.y);
    vec3 dtl = vec3(uv, 0.0) + vec3(-texel, texture(sceneDepth, uv - dv - du).x);
    vec3 dtr = vec3(uv, 0.0) + vec3(texel.x, -texel.y, texture(sceneDepth, uv - dv + du).x);
    vec3 dmc = vec3(uv, 0.0) + vec3(0.0, 0.0, texture(sceneDepth, uv).x);
    vec3 dbl = vec3(uv, 0.0) + vec3(-texel.x, texel.y, texture(sceneDepth, uv + dv - du).x);
    vec3 dbr = vec3(uv, 0.0) + vec3(texel.x, texel.y, texture(sceneDepth, uv + dv + du).x);
    vec3 dmin = dmc;
    dmin = dmin.z > dtr.z ? dtr : dmin;
    dmin = dmin.z > dtl.z ? dtl : dmin;
    dmin = dmin.z > dbl.z ? dbl : dmin;
    dmin = dmin.z > dbr.z ? dbr : dmin;
    return dmin;
}

// SMAA SIGGRAPH 2016 Fast Catmull-Rom, same weights as Bliss composite5.
vec3 SerenityFastCatmulRom(vec2 texcoord, vec2 texel) {
    vec2 invTexel = 1.0 / max(texel, vec2(1e-6));
    vec2 position = invTexel * texcoord;
    vec2 centerPosition = floor(position - 0.5) + 0.5;
    vec2 f = position - centerPosition;
    vec2 f2 = f * f;
    vec2 f3 = f * f2;
    float c = 0.75;
    vec2 w0 = -c * f3 + 2.0 * c * f2 - c * f;
    vec2 w1 = (2.0 - c) * f3 - (3.0 - c) * f2 + 1.0;
    vec2 w2 = -(2.0 - c) * f3 + (3.0 - 2.0 * c) * f2 + c * f;
    vec2 w3 = c * f3 - c * f2;
    vec2 w12 = w1 + w2;
    vec2 tc12 = texel * (centerPosition + w2 / max(w12, vec2(1e-6)));
    vec2 tc0 = texel * (centerPosition - 1.0);
    vec2 tc3 = texel * (centerPosition + 2.0);
    vec4 color = vec4(texture(serenityHistory, vec2(tc12.x, tc0.y)).rgb, 1.0) * (w12.x * w0.y)
        + vec4(texture(serenityHistory, vec2(tc0.x, tc12.y)).rgb, 1.0) * (w0.x * w12.y)
        + vec4(texture(serenityHistory, tc12).rgb, 1.0) * (w12.x * w12.y)
        + vec4(texture(serenityHistory, vec2(tc3.x, tc12.y)).rgb, 1.0) * (w3.x * w12.y)
        + vec4(texture(serenityHistory, vec2(tc12.x, tc3.y)).rgb, 1.0) * (w12.x * w3.y);
    return max(color.rgb / max(color.a, 1e-6), vec3(0.0));
}

float SerenityTaaLuma(vec3 color) {
    return max(dot(color, vec3(0.2126, 0.7152, 0.0722)), 1e-6);
}

vec3 SerenityTaaTonemap(vec3 col) {
    return col / (1.0 + SerenityTaaLuma(col));
}

vec3 SerenityTaaInvTonemap(vec3 col) {
    return col / max(1.0 - SerenityTaaLuma(col), 1e-3);
}

// Defined by the composite adapter. All neighborhood taps use the same output
// transform and linear color space as the current pixel and sRGB history image.
vec3 SerenityOutputAt(vec2 sampleUv);
vec3 SerenityTemporal(vec3 color, vec2 uv) {
    if ((int(serenity.controls[6].w + 0.5) & 2) == 0 || serenity.temporalState.y < 0.5) {
        return color;
    }
    vec2 texel = serenity.controls[6].xy;
    vec3 closest = SerenityClosestToCamera(uv, texel);
    vec3 closestHistory = SerenityReprojectHistoryUv(closest.xy, closest.z);
    vec2 velocity = closestHistory.xy - closest.xy;
    vec2 historyUv = uv + velocity;
    if (closestHistory.z < 0.5 || historyUv.x < 0.0 || historyUv.y < 0.0 || historyUv.x > 1.0 || historyUv.y > 1.0) {
        return color;
    }

    vec3 col0 = color;
    vec3 col1 = SerenityOutputAt(uv + vec2(texel.x, texel.y));
    vec3 col2 = SerenityOutputAt(uv + vec2(texel.x, -texel.y));
    vec3 col3 = SerenityOutputAt(uv + vec2(-texel.x, -texel.y));
    vec3 col4 = SerenityOutputAt(uv + vec2(-texel.x, texel.y));
    vec3 col5 = SerenityOutputAt(uv + vec2(0.0, texel.y));
    vec3 col6 = SerenityOutputAt(uv + vec2(0.0, -texel.y));
    vec3 col7 = SerenityOutputAt(uv + vec2(-texel.x, 0.0));
    vec3 col8 = SerenityOutputAt(uv + vec2(texel.x, 0.0));
    vec3 colMax = max(col0, max(col1, max(col2, max(col3, max(col4, max(col5, max(col6, max(col7, col8))))))));
    vec3 colMin = min(col0, min(col1, min(col2, min(col3, min(col4, min(col5, min(col6, min(col7, col8))))))));
    vec3 colMax5 = max(col0, max(col5, max(col6, max(col7, col8))));
    vec3 colMin5 = min(col0, min(col5, min(col6, min(col7, col8))));
    colMin = 0.5 * (colMin + colMin5);
    colMax = 0.5 * (colMax + colMax5);

    vec3 frameHistory = SerenityFastCatmulRom(historyUv, texel);
    if (SerenityTaaLuma(frameHistory) < 1e-5) {
        return color;
    }
    vec3 clampedHistory = clamp(frameHistory, colMin, colMax);
    float blendingFactor = clamp(serenity.controls[7].w, 0.02, 1.0);
    blendingFactor = mix(blendingFactor, min(blendingFactor * 2.0, 1.0), step(0.999, closest.z));
    blendingFactor = min(blendingFactor
        + SerenityTaaLuma(min(max(clampedHistory - frameHistory, vec3(0.0)) / max(frameHistory, vec3(1e-4)), vec3(1.0))),
        1.0);
    return SerenityTaaInvTonemap(mix(SerenityTaaTonemap(clampedHistory), SerenityTaaTonemap(color), blendingFactor));
}
