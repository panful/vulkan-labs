#version 450

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 outViewPos;

layout(push_constant) uniform Pushconstant{
    mat4 view;
    mat4 proj;
} PC;

layout(set = 0, binding = 0) uniform UniformBufferObject{
    mat4 model;
} UBO;

void main() 
{
    vec4 viewPos = PC.view * UBO.model * vec4(inPos, 1.);
    gl_Position  = PC.proj * viewPos;
    outViewPos   = vec3(viewPos);
}
