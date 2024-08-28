#version 450

layout(location = 0) in vec3 inViewPos;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform UniformBufferObject {
    vec3 color;
} UBO;

void main() 
{
    vec3 lightColor = UBO.color;

    vec3 dx = dFdx(inViewPos);
    vec3 dy = dFdy(inViewPos);
    vec3 normal = normalize(cross(dx, dy));
    lightColor *= max(0., -normal.z);
                        
    outColor = vec4(lightColor, 1.);
}
