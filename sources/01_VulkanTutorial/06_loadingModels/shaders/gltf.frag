#version 450

layout(location = 0) in vec3 inViewPos;
layout(location = 0) out vec4 outColor;

void main() 
{
    vec3 dx = dFdx(inViewPos);
    vec3 dy = dFdy(inViewPos);
    vec3 normal = normalize(cross(dx, dy));
    vec3 lightColor = vec3(1.);
    lightColor *= max(0., -normal.z);
                        
    outColor = vec4(lightColor, 1.);
}
