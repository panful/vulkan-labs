#version 450

layout(location = 0) in vec3 inPos;

layout(push_constant) uniform Pushconstant{
    mat4 view;
    mat4 proj;
} PC;

layout(binding = 0) uniform UniformBufferObject{
    mat4 model;
} UBO;

void main() 
{
    gl_Position = PC.proj * PC.view * UBO.model * vec4(inPos, 1.0);
}
