#version 450

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outFragColor;

layout (binding = 0) uniform sampler2D samplerColor;

layout (push_constant) uniform Pushconstant
{
    int enableHDR;
    int type;
    float exposure;
} pc;

void main() 
{
    vec4 hdrColor = texture(samplerColor, inUV);
    vec3 color    = hdrColor.rgb;

    if(pc.enableHDR == 1)
    {
        switch (pc.type)
        {
            case 0:
                color = vec3(1.0) - exp(-color * pc.exposure);  // Manual Exposure (手动曝光)
                break;
            case 1:
                color = color / (color + vec3(1.0));            // Reinhard Tone Mapping (Reinhard 色调映射)
                break;
        }
    }

    outFragColor = vec4(color, 1.0);
}
