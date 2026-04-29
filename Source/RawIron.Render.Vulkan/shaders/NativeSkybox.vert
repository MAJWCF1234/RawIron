#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 vWorldRay;

layout(std140, set = 0, binding = 0) uniform SkyUniforms {
    int hasSkyTexture;
    int _pad0;
    int _pad1;
    int _pad2;
    mat4 clipFromLocal;
    mat4 eyeToWorldRotation;
} sky;

void main() {
    vec4 clip = sky.clipFromLocal * vec4(inPosition, 1.0);
    gl_Position = clip.xyww;
    vec3 eyeDir = normalize(inPosition);
    vWorldRay = mat3(sky.eyeToWorldRotation) * eyeDir;
}
