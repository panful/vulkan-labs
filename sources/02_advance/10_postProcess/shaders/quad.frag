#version 450

layout (binding = 3) uniform sampler2D offscrrenTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

layout (push_constant) uniform Pushconstant
{
    int index;
} PC;

// 反相
vec4 Inversion()
{
   return vec4(vec3(1.0 - texture(offscrrenTex, inTexCoord)), 1.0);
}

// 灰度
vec4 Grayscale()
{
   vec4 color = texture(offscrrenTex, inTexCoord);
   float average = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
   return vec4(average, average, average, 1.0);
}

// 核效果
vec4 KernelFunc(float kernel[9])
{
   const float offset = 1.0 / 300.0;  

    vec2 offsets[9] = vec2[](
        vec2(-offset,  offset), // 左上
        vec2( 0.0f,    offset), // 正上
        vec2( offset,  offset), // 右上
        vec2(-offset,  0.0f),   // 左
        vec2( 0.0f,    0.0f),   // 中
        vec2( offset,  0.0f),   // 右
        vec2(-offset, -offset), // 左下
        vec2( 0.0f,   -offset), // 正下
        vec2( offset, -offset)  // 右下
    );

    vec3 sampleTex[9];
    for(int i = 0; i < 9; ++i)
    {
      sampleTex[i] = vec3(texture(offscrrenTex, inTexCoord.st + offsets[i]));
    }

    vec3 col = vec3(0.0);
    for(int i = 0; i < 9; ++i)
    {
      col += sampleTex[i] * kernel[i];
    }

    return vec4(col, 1.0);
}

void main()
{
    switch(PC.index)
    {
        case 0:
            outFragColor = Inversion();
            break;
        case 1:
            outFragColor = Grayscale();
            break;
        case 2:
            // 锐化
            float sharpen[9] = float[](
                -1.f, -1.f, -1.f,
                -1.f,  9.f, -1.f,
                -1.f, -1.f, -1.f
            );
            outFragColor = KernelFunc(sharpen);
            break;
        case 3:
            // 模糊
            float blur[9] = float[](
                1.f / 16.f,   2.f / 16.f,   1.f / 16.f,
                2.f / 16.f,   4.f / 16.f,   2.f / 16.f,
                1.f / 16.f,   2.f / 16.f,   1.f / 16.f  
            );
            outFragColor = KernelFunc(blur);
            break;
        case 4:
            // 边缘检测
            float edge_detection[9] = float [](
                1.f,    1.f,    1.f,
                1.f,   -8.f,    1.f,
                1.f,    1.f,    1.f  
            );
            outFragColor = KernelFunc(edge_detection);
            break;
        default:
            outFragColor = texture(offscrrenTex, inTexCoord);
            break;
    }
}