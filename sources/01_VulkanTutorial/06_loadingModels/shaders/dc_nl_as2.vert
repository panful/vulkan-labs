#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inWeight;
layout(location = 2) in vec4 inJoint;

layout(location = 0) out vec3 outViewPos;

layout(push_constant) uniform Pushconstant{
    mat4 view;
    mat4 proj;
} PC;

layout(set = 0, binding = 0) uniform UniformBufferObject_0 {
    mat4 model;
} UBO_Model;

layout(set = 1, binding = 0) readonly buffer UniformBufferObject_1 {
    mat4 jointMat[];
} UBO_Skin;

void main() 
{
    mat4 skinMat = inWeight.x * UBO_Skin.jointMat[int(inJoint.x)] +
                   inWeight.y * UBO_Skin.jointMat[int(inJoint.y)] +
                   inWeight.z * UBO_Skin.jointMat[int(inJoint.z)] +
                   inWeight.w * UBO_Skin.jointMat[int(inJoint.w)];

    vec4 tempPos = skinMat * vec4(inPos, 1.);
    vec4 viewPos = PC.view * UBO_Model.model * tempPos;
    gl_Position  = PC.proj * viewPos;
    outViewPos   = vec3(viewPos);
}
