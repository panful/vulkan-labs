#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout(push_constant) uniform PointDecodePushConstant {
    vec4 offset;
    vec4 scale;
} pointDecode;

layout(location = 0) in ivec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

void main() {
    vec3 position = vec3(inPosition) * pointDecode.scale.xyz + pointDecode.offset.xyz;
    gl_PointSize = 1.0;
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(position, 1.0);
    fragColor = inColor;
}
