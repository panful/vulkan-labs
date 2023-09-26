#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(constant_id = 0) const int COLOR_MODE = 0;

void main() 
{
    switch(COLOR_MODE)
    {
        case 0:
            outColor = vec4(1.0, 0.0, 0.0, 1.0);
            break;
        case 1:
            outColor = vec4(0.0, 1.0, 0.0, 1.0);
            break;
        case 2:
            outColor = vec4(0.0, 0.0, 1.0, 1.0);
            break;
        default:
            outColor = vec4(1.0);
            break;
    }
    
}