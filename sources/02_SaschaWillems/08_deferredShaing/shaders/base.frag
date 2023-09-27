#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outPosition;

layout(binding = 6) uniform sampler2D texSampler;

void main() 
{
    outColor = texture(texSampler, fragTexCoord);
    outNormal = vec4(normalize(fragNormal), 1.0);
    outPosition = vec4(fragPos, 1.0);
}