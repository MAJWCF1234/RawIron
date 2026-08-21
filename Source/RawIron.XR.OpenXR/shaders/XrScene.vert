#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec4 inAtlasRect;
layout(location = 5) in vec4 inNormalAtlasRect;
layout(location = 6) in vec4 inMaterialParams;
layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outColor;
layout(location = 3) out vec2 outTexCoord;
layout(location = 4) flat out vec4 outAtlasRect;
layout(location = 5) flat out vec4 outNormalAtlasRect;
layout(location = 6) flat out vec4 outMaterialParams;

layout(push_constant) uniform EyeData {
    mat4 viewProjection;
    vec4 cameraWorldPosition;
} eye;

void main() {
    gl_Position = eye.viewProjection * vec4(inPosition, 1.0);
    outWorldPosition = inPosition;
    outNormal = inNormal;
    outColor = inColor;
    outTexCoord = inTexCoord;
    outAtlasRect = inAtlasRect;
    outNormalAtlasRect = inNormalAtlasRect;
    outMaterialParams = inMaterialParams;
}
