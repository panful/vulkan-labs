#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inputNormal;
layout (input_attachment_index = 2, binding = 2) uniform subpassInput inputPosition;
layout (input_attachment_index = 3, binding = 3) uniform subpassInput inputDepth;

layout(push_constant) uniform Pushconstant{
    int index;
} pc_index;

struct Light{
    vec4 position_shininess;    // 光源位置、反光度（高光指数）
    vec4 color_radius;          // 光源颜色、光源衰减
};

const int MAX_LIGHT_NUM = 8;

layout (binding = 7) uniform LightBlock
{
    vec4 viewPos;
	Light light[MAX_LIGHT_NUM];
} uboLight;

layout (location = 0) out vec4 outFragColor;

void main() 
{
    if(0 == pc_index.index)
    {
        outFragColor = subpassLoad(inputColor);
    }
    else if(1 == pc_index.index)
    {
        outFragColor = subpassLoad(inputNormal);
    }
    else if(2 == pc_index.index)
    {
        outFragColor = vec4(subpassLoad(inputDepth).r);
    }
    else if(3 == pc_index.index)
    {
        vec3 FragPos = subpassLoad(inputPosition).xyz;
        vec3 Normal  = normalize(subpassLoad(inputNormal).xyz);
        vec3 Color   = subpassLoad(inputColor).rgb;

        vec3 viewDir  = normalize(uboLight.viewPos.xyz - FragPos);
        vec3 tmpColor = Color * 0.1;

        for(int i = 0; i < MAX_LIGHT_NUM; ++i)
        {
            vec3 lightColor    = uboLight.light[i].color_radius.rgb;
            vec3 lightPosition = uboLight.light[i].position_shininess.xyz;
            float shininess    = uboLight.light[i].position_shininess.w;
            float radius       = uboLight.light[i].color_radius.a;

            // 漫反射
            vec3 lightDir  = normalize(lightPosition - FragPos);
            vec3 diffuse  = max(dot(Normal, lightDir), 0.0) * Color * lightColor;

            // 镜面反射
            vec3 halfwayDir = normalize(lightDir + viewDir);
            vec3 specular  = pow(max(dot(Normal, halfwayDir), 0.0), shininess) * lightColor;

            // 衰减
            float len = length(lightPosition - FragPos);
		    float atten = radius / (pow(len, 2.0) + 1.0);

            tmpColor += (diffuse +specular) * atten;
        }

        outFragColor = vec4(tmpColor, 1.0);
    }
    else
    {
        outFragColor = vec4(1.0);
    }
}
