#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inputDepth;

layout (location = 0) out vec4 outFragColor;

void main() 
{
    //outFragColor = vec4(1.0, 0.0, 0.0, 1.0);
    //outFragColor = subpassLoad(inputColor);
    outFragColor = vec4(subpassLoad(inputDepth).r);
}
