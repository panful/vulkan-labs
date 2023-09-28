#version 450

layout (binding = 3) uniform sampler2D offscrrenTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = texture(offscrrenTex, inTexCoord);
}