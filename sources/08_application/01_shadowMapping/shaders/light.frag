#version 450

layout(location = 0) in vec4 inPosInLightSpace;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D shadowMap;

float shadowCalculation()
{
    vec3 projPos = inPosInLightSpace.xyz / inPosInLightSpace.w;
    float closestDepth = texture(shadowMap, projPos.xy).r;
    float currentDepth = projPos.z;

    return (currentDepth - 0.000001) > closestDepth ? 1.0 : 0.0;
}

void main() 
{
    vec3 color = (1.0 - shadowCalculation()) * vec3(1.0, 0.0, 0.0);
    outColor = vec4(color, 1.0);
}
