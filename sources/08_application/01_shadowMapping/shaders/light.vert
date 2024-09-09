#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outPosInLightSpace;

layout(binding = 0) uniform UniformBufferObject{
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightSpace;
} ubo;

// [-1,1] -> [0,1]
const mat4 convertMat = mat4( 
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0
);

void main() 
{
    vec4 modelPos = ubo.model * vec4(inPos, 1.0);

    gl_Position         = ubo.proj * ubo.view * modelPos;
    outPosInLightSpace  = convertMat * ubo.lightSpace * modelPos;
}
