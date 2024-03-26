#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inColor2;
layout(location = 3) in vec2 inOffset;

layout(location = 0) out vec3 fragColor;

void main() 
{
    vec2 pos = inPos + inOffset;
    gl_Position = vec4(pos, 0.f, 1.f);
    fragColor = inColor2;
}
