#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec3 outViewPos;
layout(location = 1) out vec2 outTexCoord;

layout(push_constant) uniform Pushconstant{
    mat4 view;
    mat4 proj;
} PC;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
} UBO_Model;

void main() 
{
    vec4 viewPos = PC.view * UBO_Model.model * vec4(inPos, 1.);
    gl_Position  = PC.proj * viewPos;
    outViewPos   = vec3(viewPos);
    outTexCoord  = inTexCoord;
}
