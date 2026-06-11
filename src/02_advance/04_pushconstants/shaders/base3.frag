#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Pushconstant{
    layout(offset = 16) vec3 frag_color; // offset 表示的是顶点着色器中的 PushConstant 大小，需要考虑字节对齐
} PC;

void main() 
{
    outColor = vec4(fragColor + PC.frag_color, 1.0);
}
