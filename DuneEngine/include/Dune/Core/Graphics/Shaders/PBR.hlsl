#include "PBR.h"

#define PI 3.141592653589793115997963468544185161590576171875f

ConstantBuffer<PBRGlobals> globals : register(b0);
ConstantBuffer<PBRInstance> instance : register(b1);

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
};


VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT o;
    
    float4 wPos = mul(instance.modelMatrix, float4(input.position, 1.0f));
    o.position = mul(globals.viewProjectionMatrix, wPos);
    o.normal = normalize(mul((float3x3)instance.modelMatrix, input.normal));
    return o;
}

ConstantBuffer<PBRMaterial> material : register(b1);

struct PS_INPUT
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
};

struct PS_OUTPUT
{
    float4 color : SV_TARGET;
};

PS_OUTPUT PSMain(PS_INPUT input)
{
    PS_OUTPUT output;
    const float3 l = normalize(-globals.sunDirection);
    const float3 v = normalize(mul((float4x3) globals.viewProjectionMatrix, float3(0.0f, 0.0f, 1.0f))).xyz;
    const float3 n = normalize(input.normal);
    const float3 h = normalize(l + v);
    
    const float nDotL = saturate(dot(input.normal, -globals.sunDirection));
    const float nDotV = saturate(dot(input.normal, v));
    const float nDotH = saturate(dot(n, h));
    const float vDotH = saturate(dot(v, h));
    const float alpha = material.roughness * material.roughness;
    const float alpha2 = alpha * alpha;
    const float k = pow(material.roughness + 1.0f, 2.0f) / 8.0f;
    
    const float D = alpha2 / pow((PI * nDotH * nDotH * (alpha2 - 1.0f) + 1.0f), 2.0f);
    const float G = (nDotL / (nDotL * (1.0f - k) + k)) * (nDotV / (nDotV * (1.0f - k) + k));    
    const float F = 0.04f + (0.96f * pow(1.0f - vDotH, 5.0f));
    
    const float3 diffuse = (material.albedo / PI);
    const float specular = (D * F * G) / max(4.0f * nDotL * nDotV, 0.000001f);
    
    const float3 BRDF = (diffuse + specular) * nDotL;
    const float3 BRDFGammaCorrected = pow(BRDF, 1.0f / 2.2f);
    output.color = float4(BRDFGammaCorrected, 1.0f);
    return output;
}