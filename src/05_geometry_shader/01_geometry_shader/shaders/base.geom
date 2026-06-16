#version 450

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 inColor[];
layout(location = 0) out vec3 outColor;

void main()
{
    // 注意输出顶点的顺序（逆时针为正面，背面会被裁剪）
    gl_Position = gl_in[0].gl_Position + vec4(0.0, 0.3, 0.0, 0.0);
    outColor = inColor[0];
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(0.3, -0.3, 0.0, 0.0);
    outColor = inColor[0];
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(-0.3, -0.3, 0.0, 0.0);
    outColor = inColor[0];
    EmitVertex();

    EndPrimitive();
}
