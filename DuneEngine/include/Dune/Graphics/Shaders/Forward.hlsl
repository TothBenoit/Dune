#include "ShaderTypes.h"
#include "Lighting.hlsli"


struct DirectionalLights
{
    DirectionalLight lights[255];
};

struct PointLights
{
    PointLight lights[255];
};

ConstantBuffer<ForwardGlobals> cGlobals : register(b0);
ConstantBuffer<DirectionalLights> cDirectionalLights : register(b1);
ConstantBuffer<PointLights> cPointLights : register(b2);
ConstantBuffer<InstanceData> cInstance : register(b1);

Texture2D tAlbedo : register(t0);
Texture2D tNormal : register(t1);
Texture2D tRoughnessMetalness : register(t2);

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : UV;
};

struct VSToPS
{
    float4 position : SV_Position;
    float3 worldPosition : WORLDPOSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : UV;
};

VSToPS VSMain(VS_INPUT input)
{
    VSToPS o;
    
    float4 worldPosition = mul(cInstance.modelMatrix, float4(input.position, 1.0f));
    o.worldPosition = worldPosition.xyz;
    o.position = mul(cGlobals.viewProjectionMatrix, worldPosition);
    o.normal = normalize(mul((float3x3) cInstance.modelMatrix, input.normal));
    o.tangent = float4(mul((float3x3) cInstance.modelMatrix, input.tangent), dot(input.tangent, input.tangent) > 0.5 ? 1. : -1.);
    o.uv = input.uv;
    return o;
}

struct PS_OUTPUT
{
    float4 color : SV_TARGET;
};

PS_OUTPUT PSMain(VSToPS input)
{
    PS_OUTPUT output;
    const float3 albedo = tAlbedo.Sample(sAnisoWrap, input.uv).rgb;
    const float3 nf = tNormal.Sample(sAnisoWrap, input.uv).rgb * 2.0f - 1.0f;
    const float2 roughnessMetalness = tRoughnessMetalness.Sample(sAnisoWrap, input.uv).gb;

    const float3 tanX = input.tangent.xyz;
    const float3 tanY = cross(input.normal, tanX) * -input.tangent.w;
    const float3 n = normalize(nf.x * tanX + nf.y * tanY + nf.z * input.normal);
    const float3 v = normalize(cGlobals.cameraPosition - input.worldPosition);

    float3 directLighting = 0.f.xxx;
    for (int directionalIndex = 0; directionalIndex < cGlobals.directionalLightCount; directionalIndex++)
        directLighting += Light(cDirectionalLights.lights[directionalIndex], n, v, albedo, roughnessMetalness.x, roughnessMetalness.y);
 
    for (int pointIndex = 0; pointIndex < cGlobals.pointLightCount; pointIndex++)
        directLighting += Light(cPointLights.lights[pointIndex], n, v, input.worldPosition, albedo, roughnessMetalness.x, roughnessMetalness.y);

    const float3 indirectLighting = DiffuseLambert(albedo) * cGlobals.ambientColor;
    const float3 BRDF = directLighting + indirectLighting;
    const float3 BRDFGammaCorrected = pow(BRDF, 1.0f / 2.2f);    
    output.color = float4(BRDFGammaCorrected, 1.0f);
    return output;
}