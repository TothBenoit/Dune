#include "BRDF.hlsli"
#include "PBR.h"

ConstantBuffer<PBRGlobals> cGlobals : register(b0);

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float2 uv       : UV;
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv       : UV;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT o;
     
    o.position = mul(cGlobals.viewProjectionMatrix, float4(input.position, 1.0));
    o.normal = input.normal;
    o.tangent = float4(input.tangent, dot(input.tangent, input.tangent) > 0.5 ? 1. : -1.);
    o.uv = input.uv;
    return o;
}

struct PS_INPUT
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv       : UV;
};

struct PS_OUTPUT
{
    float4 color : SV_TARGET;
};

static const float3 sunDirection = float3(0.1f, -1.0f, 0.9f);

PS_OUTPUT PSMain(PS_INPUT input)
{
    PS_OUTPUT output;
    const float3 l = normalize(-sunDirection);
    const float3 v = normalize(mul( cGlobals.viewProjectionMatrix, float4(0.0f, 0.0f, 1.0f, 1.0))).xyz;
    const float3 nf = float3(0.0f, 0.0f, 1.0f);
    const float3 tanX = input.tangent.xyz;
    const float3 tanY = cross(input.normal, tanX) * -input.tangent.w;
    
    const float3 n = normalize(nf.x * tanX + nf.y * tanY + nf.z * input.normal);
    const float3 h = normalize(l + v);
    
    const float nDotL = saturate(dot(n, -sunDirection));
    const float nDotV = saturate(dot(n, v));
    const float nDotH = saturate(dot(n, h));
    const float vDotH = saturate(dot(v, h));
    const float roughness = 1.0f;
    const float alpha = roughness * roughness;
    const float alpha2 = alpha * alpha;
    
    const float D = NormalDistributionGGX(alpha2, nDotH);
    const float Vis = VisGeometrySchlickGGX(nDotV, nDotL, roughness);
    const float3 F = FresnelSchlick(vDotH, float3(1.0f, 1.0f, 1.0f));
    
    const float3 albedo = float3(1.0f, 1.0f, 1.0f);
    const float3 diffuse = DiffuseLambert(albedo);
    const float3 specular = (D * F * Vis);
    
    const float3 BRDF = (diffuse + specular) * nDotL + 0.1f;
    const float3 BRDFGammaCorrected = pow(BRDF, 1.0f / 2.2f);
    
    output.color = float4(BRDFGammaCorrected, 1.0f);
    return output;
}