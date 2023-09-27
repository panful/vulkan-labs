#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inputNormal;
layout (input_attachment_index = 2, binding = 2) uniform subpassInput inputPosition;
layout (input_attachment_index = 3, binding = 3) uniform subpassInput inputDepth;

layout(push_constant) uniform Pushconstant{
    int index;
} pc_index;

layout (location = 0) out vec4 outFragColor;

void main() 
{
    if(0 == pc_index.index)
    {
        outFragColor = subpassLoad(inputColor);
    }
    else if(1 == pc_index.index)
    {
        outFragColor = subpassLoad(inputNormal);
    }
    else if(2 == pc_index.index)
    {
        outFragColor = vec4(subpassLoad(inputDepth).r);
    }
    else
    {
        outFragColor = vec4(1.0);
    }
}
