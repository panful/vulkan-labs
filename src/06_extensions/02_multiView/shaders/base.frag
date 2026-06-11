#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2DArray samplerView;
layout(constant_id = 0) const float VIEW_LAYER = 0.f;

void main() 
{
    outColor = vec4(vec3(texture(samplerView, vec3(inUV, VIEW_LAYER))), 1.f);
}
