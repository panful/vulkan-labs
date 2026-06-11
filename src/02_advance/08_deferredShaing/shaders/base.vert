#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

layout(binding = 9) uniform UniformBufferObject{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() 
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);

    fragPos = vec3(ubo.model * vec4(inPos, 1.0));
    fragNormal = normalize(mat3(transpose(inverse(ubo.model))) * inNormal);
    fragTexCoord = inTexCoord;
}