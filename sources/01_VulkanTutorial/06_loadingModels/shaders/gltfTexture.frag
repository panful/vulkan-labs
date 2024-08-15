#version 450

layout(location = 0) in vec3 inViewPos;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

void main() 
{
    vec3 lightColor = vec3(inTexCoord.x, inTexCoord.y, 0.);
    
    vec3 dx = dFdx(inViewPos);
    vec3 dy = dFdy(inViewPos);
    vec3 normal = normalize(cross(dx, dy));
    lightColor *= max(0., -normal.z);
                        
    outColor = vec4(lightColor, 1.);
}
