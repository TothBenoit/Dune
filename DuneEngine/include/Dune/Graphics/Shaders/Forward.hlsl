#include "ShaderInterop.h"
#include "Lighting.hlsli"

ConstantBuffer<ForwardGlobals> cGlobals : register(b0);
ConstantBuffer<InstanceData> cInstance : register(b1);

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

ConstantBuffer<MaterialData> cMaterial : register(b1);

struct PS_OUTPUT
{
    float4 color : SV_TARGET;
};

PS_OUTPUT PSMain(VSToPS input)
{
    PS_OUTPUT output;
    float3 albedo = cMaterial.baseColor;
    if (IsValid(cMaterial.albedoIdx))
    {
        Texture2D albedoTexture = ResourceDescriptorHeap[cMaterial.albedoIdx];
        albedo *= albedoTexture.Sample(sAnisoWrap, input.uv).rgb;
    }

    float3 n = input.normal;
    if (IsValid(cMaterial.normalIdx))
    {
        Texture2D normalTexture = ResourceDescriptorHeap[cMaterial.normalIdx];
        const float2 sampledNormal = normalTexture.Sample(sAnisoWrap, input.uv).xy;
        const float3x3 TBN = TangentToWorld(input.normal, float4(normalize(input.tangent.xyz), input.tangent.w));
        const float3 nf = UnpackNormal(sampledNormal);
        n = mul(nf, TBN);
    }

    float roughness = cMaterial.roughnessFactor;
    float metalness = cMaterial.metalnessFactor;
    if (IsValid(cMaterial.roughnessMetalnessIdx))
    {
        Texture2D roughnessMetalnessTexture = ResourceDescriptorHeap[cMaterial.roughnessMetalnessIdx];
        const float2 roughnessMetalness = roughnessMetalnessTexture.Sample(sAnisoWrap, input.uv).gb;
        roughness *= roughnessMetalness.x;
        metalness *= roughnessMetalness.y;
    }

    const float3 v = normalize(cGlobals.cameraPosition - input.worldPosition);
    const float3 f0 = ComputeF0(0.04.xxx, albedo, metalness);
    const float3 diffuseColor = albedo * (1.0 - metalness);
 
    float3 directLighting = 0.f.xxx;
    StructuredBuffer<Light> lights = ResourceDescriptorHeap[cGlobals.lightBufferIndex];
    for (int lightIndex = 0; lightIndex < cGlobals.lightCount; lightIndex++)
    {
        Light light = lights[lightIndex];
        directLighting += ComputeLight(light, cGlobals.lightMatricesIndex, n, v, input.worldPosition, diffuseColor, f0, roughness);
    }

    output.color = float4(directLighting, 1.0f);
    return output;
}