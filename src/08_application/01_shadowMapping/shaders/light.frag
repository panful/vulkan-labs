#version 450

layout(location = 0) in vec4 inPosInLightSpace;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D shadowMap;

layout(push_constant) uniform Pushconstant{
    int enablePCF;
} PC;

float shadowCalculation(vec4 shadowCoord, vec2 off)
{
    float closestDepth = texture(shadowMap, shadowCoord.xy + off).r;
    float currentDepth = shadowCoord.z;

    return (currentDepth - 0.000001) > closestDepth ? 0.1 : 1.0;
}

float filterPCF(vec4 shadowCoord)
{
    ivec2 texDim = textureSize(shadowMap, 0);
    float scale = 1.5;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);

    float shadowFactor = 0.0;
    int count = 0;
    int range = 1;
    
    for (int x = -range; x <= range; x++)
    {
        for (int y = -range; y <= range; y++)
        {
            shadowFactor += shadowCalculation(shadowCoord, vec2(dx*x, dy*y));
            count++;
        }
    
    }
    return shadowFactor / count;
}

void main() 
{
    vec4 pos = inPosInLightSpace / inPosInLightSpace.w;
    float shadow = PC.enablePCF == 1 ? filterPCF(pos) : shadowCalculation(pos, vec2(0.0));

    vec3 color = shadow * vec3(1.0, 0.0, 0.0);
    outColor = vec4(color, 1.0);
}
