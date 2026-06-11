#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform UniformBufferObject0 {
    vec3 color;
} uboColor;

layout(set = 1, binding = 0) uniform UniformBufferObject1 {
    mat4 model;
    mat4 view;
    mat4 proj;
} uboMVP;

void main() 
{
    gl_Position = uboMVP.proj * uboMVP.view * uboMVP.model * vec4(inPos, 0.0, 1.0);
    fragColor = uboColor.color;
}
