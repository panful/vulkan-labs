#version 450

layout(location = 0) out vec4 outFragColor;

// 点光源透视投影的近远平面
float near = 0.1f;
float far  = 100.f;

// 如果是点光源，需要将非线性的深度值转换为线性（因为点光源使用的是透视投影矩阵）
float LinearizeDepth(float depth)
{
    return (depth * (near + far) - near) / (far - near);
}

void main() 
{
    outFragColor = vec4(1.0);

    gl_FragDepth = LinearizeDepth(gl_FragCoord.z);
}
