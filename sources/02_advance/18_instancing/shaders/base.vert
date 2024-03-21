#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() 
{
    float x = inPos.x + .3f * gl_InstanceIndex;
    gl_Position = vec4(x, inPos.y, 0.f, 1.f);
    fragColor = inColor / (gl_InstanceIndex + 1.f); // gl_InstanceIndex 从0开始，加1避免除0错误
}
