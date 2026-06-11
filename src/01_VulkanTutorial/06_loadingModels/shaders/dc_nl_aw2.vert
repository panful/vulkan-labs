#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inPos1;
layout(location = 2) in vec3 inPos2;

layout(location = 0) out vec3 outViewPos;

layout(push_constant) uniform Pushconstant{
    mat4 view;
    mat4 proj;
} PC;

layout(set = 0, binding = 0) uniform UniformBufferObject_0 {
    mat4 model;
} UBO_Model;

layout(set = 1, binding = 0) uniform UniformBufferObject_1 {
    float weight1;
    float weight2;
} UBO_Morph;

void main() 
{
    vec3 tempPos = inPos + UBO_Morph.weight1 * inPos1 + UBO_Morph.weight2 * inPos2;
    vec4 viewPos = PC.view * UBO_Model.model * vec4(tempPos, 1.);
    gl_Position  = PC.proj * viewPos;
    outViewPos   = vec3(viewPos);
}
