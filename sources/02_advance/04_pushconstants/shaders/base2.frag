#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Pushconstant{
    vec3 vert_color;
    vec3 frag_color;
} PC;

void main() 
{
    outColor = vec4(fragColor + PC.frag_color, 1.0);
}
