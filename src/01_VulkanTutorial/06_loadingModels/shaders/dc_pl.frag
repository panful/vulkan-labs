#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inViewPos;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform UniformBufferObject_1 {
    float roughness;
    float metallic;
} UBO_PBR;

layout(set = 2, binding = 0) uniform UniformBufferObject_2 {
    vec3 color;
} UBO_Color;

layout(push_constant) uniform Pushconstant{
    layout (offset = 128) vec3 cameraPos;
} PC;

const float PI = 3.14159265359;

vec3 lights[4] = vec3[4](
    vec3(3.0, 0.0, -3.0),
    vec3(0.0, 3.0, -3.0),
    vec3(0.0, 0.0, -3.0),
    vec3(3.0, 3.0, -3.0)
);

//----------------------------------------------------------------
// 菲涅尔方程 F
// 在不同的表面角下表面所反射的光线所占的比率
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// 法线分布函数 D
// 估算在受到表面粗糙度的影响下，朝向方向与半程向量一致的微平面的数量。这是用来估算微平面的主要函数
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
          denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

// 几何遮蔽函数 G
// 描述了微平面自成阴影的属性。当一个平面相对比较粗糙的时候，平面表面上的微平面有可能挡住其他的微平面从而减少表面所反射的光线
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

void main() 
{
    vec3 N = normalize(inNormal);
    vec3 V = normalize(PC.cameraPos - inWorldPos);

    // 非金属表面F0始终为0.04
    // 金属表面根据初始的F0和表现金属属性的反射率进行线性插值
    vec3 F0 = vec3(0.04);
         F0 = mix(F0, UBO_Color.color, UBO_PBR.metallic);

    vec3 Lo = vec3(0.0);
    for(int i = 0; i < 4; ++i)
    {
        //--------------------------------------------------------------
        // 光源方向和半程向量
        vec3 L            = normalize(lights[i] - inWorldPos);
        vec3 H            = normalize(V + L);

        //--------------------------------------------------------------
        // 衰减
        float distance    = length(lights[i] - inWorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance     = vec3(100.0) * attenuation; // lightColor = vec3(100.0)

        //--------------------------------------------------------------
        // 双向反射分布函数 Cook-Torrance specular BRDF
        float D = DistributionGGX(N, H, UBO_PBR.roughness);
        float G = GeometrySmith(N, V, L, UBO_PBR.roughness);
        vec3  F = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

        vec3  numerator   = D * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // 加0.0001保证分母大于0
        vec3  specular    = numerator / denominator;

        //--------------------------------------------------------------
        // 光源被反射的能量比例
        vec3 KS = F;
        // 折射
        vec3 KD = vec3(1.0) - KS;
        // 金属表面不会折射，即metallic为1时，KD = 0
        KD *= 1.0 - UBO_PBR.metallic;

        //--------------------------------------------------------------
        // 将所有光源的出射光线辐射率加起来
        float NdotL = max(dot(N, L), 0.0);
        Lo += (KD * UBO_Color.color / PI + specular) * radiance * NdotL; // BRDF已经乘过KS，所以此处不需要再乘以KS
    }

    // 环境光照
    vec3 ambient = vec3(0.03) * UBO_Color.color;
    vec3 color   = ambient + Lo;

    // HDR色调映射
    color = color / (color + vec3(1.0));
    // gamma校正
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}  

// void main() 
// {
//     vec3 lightColor = UBO_Color.color;
    
//     vec3 dx = dFdx(inViewPos);
//     vec3 dy = dFdy(inViewPos);
//     vec3 normal = normalize(cross(dx, dy));
//     lightColor *= max(0., -normal.z);
                        
//     outColor = vec4(lightColor, 1.);
// }
