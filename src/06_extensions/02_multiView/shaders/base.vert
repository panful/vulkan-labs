#version 450

layout(location = 0) out vec2 textureUV;

vec2 positions[6] = vec2[](
    vec2(-.5f, -.5f),
    vec2(-.5f,  .5f),
    vec2( .5f, -.5f),

    vec2(-.5f,  .5f),
    vec2( .5f,  .5f),
    vec2( .5f, -.5f)
);

vec2 UV[6] = vec2[](
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),

    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.f, 1.f);
    textureUV = UV[gl_VertexIndex];
}
