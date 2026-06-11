#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outViewPos;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec2 outTexCoord;

layout(push_constant) uniform Pushconstant{
    mat4 view;
    mat4 proj;
} PC;

layout(set = 0, binding = 0) uniform UniformBufferObject_0 {
    mat4 model;
} UBO_Model;

void main() 
{
    vec4 worldPos = UBO_Model.model * vec4(inPos, 1.);
    vec4 viewPos  = PC.view * worldPos;

    gl_Position = PC.proj * viewPos;

    outNormal    = mat3(transpose(inverse(UBO_Model.model))) * inNormal;
    outViewPos   = vec3(viewPos);
    outWorldPos  = vec3(worldPos);
    outTexCoord  = inTexCoord;
}
