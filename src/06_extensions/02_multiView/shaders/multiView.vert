#version 450

#extension GL_EXT_multiview : enable

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() 
{
    gl_Position = vec4(inPos, 0.0, 1.0);

    // 对不同的视图（图层）输出不同的颜色
    if(gl_ViewIndex == 0)
    {
        fragColor = vec3(1.f) - inColor;
    }
    else
    {
        fragColor = inColor;
    }
}
