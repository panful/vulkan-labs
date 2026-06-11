#version 450

layout (binding = 3) uniform sampler2D offscrrenTex;

layout (location = 0) in vec2 inTexCoord;
layout (location = 0) out vec4 outFragColor;

layout (push_constant) uniform Pushconstant
{
    vec2 texel;
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

//Maximum texel span
#define SPAN_MAX   (8.0)
//These are more technnical and probably don't need changing:
//Minimum "dir" reciprocal
#define REDUCE_MIN (1.0/128.0)
//Luma multiplier for "dir" reciprocal
#define REDUCE_MUL (1.0/32.0)

// https://github.com/XorDev/GM_FXAA/blob/main/GM_FXAA/shaders/shd_fxaa/shd_fxaa.fsh
vec4 textureFXAA(sampler2D tex, vec2 uv)
{
	//Sample center and 4 corners
    vec3 rgbCC = texture(tex, uv).rgb;
    vec3 rgb00 = texture(tex, uv+vec2(-0.5,-0.5)*PC.texel).rgb;
    vec3 rgb10 = texture(tex, uv+vec2(+0.5,-0.5)*PC.texel).rgb;
    vec3 rgb01 = texture(tex, uv+vec2(-0.5,+0.5)*PC.texel).rgb;
    vec3 rgb11 = texture(tex, uv+vec2(+0.5,+0.5)*PC.texel).rgb;
	
	//Luma coefficients
    const vec3 luma = vec3(0.299, 0.587, 0.114);
	//Get luma from the 5 samples
    float lumaCC = dot(rgbCC, luma);
    float luma00 = dot(rgb00, luma);
    float luma10 = dot(rgb10, luma);
    float luma01 = dot(rgb01, luma);
    float luma11 = dot(rgb11, luma);
	
	//Compute gradient from luma values
    vec2 dir = vec2((luma01 + luma11) - (luma00 + luma10), (luma00 + luma01) - (luma10 + luma11));
	//Diminish dir length based on total luma
    float dirReduce = max((luma00 + luma10 + luma01 + luma11) * REDUCE_MUL, REDUCE_MIN);
	//Divide dir by the distance to nearest edge plus dirReduce
    float rcpDir = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	//Multiply by reciprocal and limit to pixel span
    dir = clamp(dir * rcpDir, -SPAN_MAX, SPAN_MAX) * PC.texel.xy;
	
	//Average middle texels along dir line
    vec4 A = 0.5 * (
        texture(tex, uv - dir * (1.0/6.0)) +
        texture(tex, uv + dir * (1.0/6.0)));
	
	//Average with outer texels along dir line
    vec4 B = A * 0.5 + 0.25 * (
        texture(tex, uv - dir * (0.5)) +
        texture(tex, uv + dir * (0.5)));
		
		
	//Get lowest and highest luma values
    float lumaMin = min(lumaCC, min(min(luma00, luma10), min(luma01, luma11)));
    float lumaMax = max(lumaCC, max(max(luma00, luma10), max(luma01, luma11)));
    
	//Get average luma
	float lumaB = dot(B.rgb, luma);
	//If the average is outside the luma range, using the middle average
    return ((lumaB < lumaMin) || (lumaB > lumaMax)) ? A : B;
}

void main()
{
    switch(PC.index)
    {
        case 0:
            // 反向
            outFragColor = Inversion();
            break;
        case 1:
            // 灰度
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
        case 5:
            // FXAA
            outFragColor = textureFXAA(offscrrenTex, inTexCoord);
            break;
        default:
            outFragColor = texture(offscrrenTex, inTexCoord);
            break;
    }
}
