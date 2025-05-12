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
ConstantBuffer<InstanceData> cInstance : register(b1);
ConstantBuffer<MaterialData> cMaterial : register(b1);

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
    Texture2D albedoTexture = ResourceDescriptorHeap[cMaterial.albedoIdx];
    const float3 albedo = cMaterial.baseColor * albedoTexture.Sample(sAnisoWrap, input.uv).rgb;

    Texture2D normalTexture = ResourceDescriptorHeap[cMaterial.normalIdx];
    const float2 sampledNormal = normalTexture.Sample(sAnisoWrap, input.uv);

    Texture2D roughnessMetalnessTexture = ResourceDescriptorHeap[cMaterial.roughnessMetalnessIdx];
    const float2 roughnessMetalness = roughnessMetalnessTexture.Sample(sAnisoWrap, input.uv).gb;

    const float3x3 TBN = TangentToWorld(input.normal, float4(normalize(input.tangent.xyz), input.tangent.w));
    const float3 nf = UnpackNormal(sampledNormal);
    const float3 n = mul(nf, TBN);
    
    const float3 v = normalize(cGlobals.cameraPosition - input.worldPosition);

    const float roughness = cMaterial.roughnessFactor * roughnessMetalness.r;
    const float metalness = cMaterial.metalnessFactor * roughnessMetalness.g;
    const float3 f0 = ComputeF0(0.04.xxx, albedo, metalness);
    const float3 diffuseColor = albedo * (1.0 - metalness);
 
    float3 directLighting = 0.f.xxx;
    StructuredBuffer<DirectionalLight> directionalLights = ResourceDescriptorHeap[cGlobals.directionalLightBufferIndex];
    for (int directionalIndex = 0; directionalIndex < cGlobals.directionalLightCount; directionalIndex++)
        directLighting += Light(directionalLights[directionalIndex], n, v, input.worldPosition, diffuseColor, f0, roughness);

    StructuredBuffer<PointLight> pointLights = ResourceDescriptorHeap[cGlobals.pointLightBufferIndex];
    for (int pointIndex = 0; pointIndex < cGlobals.pointLightCount; pointIndex++)
        directLighting += Light(pointLights[pointIndex], n, v, input.worldPosition, diffuseColor, f0, roughness);

    const float3 indirectLighting = DiffuseLambert(albedo) * cGlobals.ambientColor;
    output.color = float4(directLighting + indirectLighting, 1.0f);
    return output;
}