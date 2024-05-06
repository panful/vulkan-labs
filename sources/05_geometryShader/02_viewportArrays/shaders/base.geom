#version 450

layout(triangles, invocations = 2) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 inColor[];
layout(location = 0) out vec3 outColor;

void main()
{
    gl_ViewportIndex = gl_InvocationID;

    if(0 == gl_InvocationID)
    {
        for (int i = 0; i < gl_in.length(); i++)
        {
            outColor = inColor[i];
            gl_Position = gl_in[i].gl_Position;
            EmitVertex();
        }
    }
    else
    {
        for (int i = 0; i < gl_in.length(); i++)
        {
            outColor = vec3(1.f) - inColor[i];
            gl_Position = gl_in[i].gl_Position;
            EmitVertex();
        }
    }

    EndPrimitive();
}
