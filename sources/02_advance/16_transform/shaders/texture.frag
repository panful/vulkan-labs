#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

void main() 
{
    vec4 color = texture(texSampler, inTexCoord);
    outColor = vec4(color.xyz, 1.0);
}
