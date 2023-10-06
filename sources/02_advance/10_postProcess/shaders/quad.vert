#version 450

layout (location = 0) in vec4 inPosition_TexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

layout (location = 0) out vec2 outTexCoord;

void main() 
{
    outTexCoord = inPosition_TexCoord.zw;
    gl_Position = vec4(inPosition_TexCoord.xy, 0.0f, 1.0f);
}
