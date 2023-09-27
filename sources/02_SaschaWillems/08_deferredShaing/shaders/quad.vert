#version 450

layout (location = 0) in vec2 inPosition;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() 
{
    gl_Position = vec4(inPosition, 0.0f, 1.0f);
}
