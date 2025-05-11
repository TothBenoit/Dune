#include "ShaderTypes.h"
#include "BRDF.hlsli"

float Shadow(DirectionalLight light, float3 worldPosition, float3 n, float nDotL)
{
    float4 lightPos = mul(light.viewProjection, float4(worldPosition, 1.f));
    lightPos.xyz /= lightPos.w;

    const float2 uv = lightPos.xy * float2(0.5f, -0.5f) + 0.5f;
    Texture2D shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowIndex)];

    return shadowMap.SampleCmpLevelZero(sLinearClampComparisonGreater, uv, lightPos.z);
}

float3 Light(DirectionalLight light, float3 n, float3 v, float3 worldPosition, float3 albedo, float3 f0, float roughness, float metalness)
{
    const float3 l = normalize(-light.direction);
    const float3 h = normalize(l + v);
        
    const float nDotL = saturate(dot(n, l));
    const float nDotV = saturate(dot(n, v));
    const float nDotH = saturate(dot(n, h));
    const float vDotH = saturate(dot(v, h));
    
    const float alpha = roughness * roughness;
    const float alpha2 = alpha * alpha;
    
    const float D = NormalDistributionGGX(alpha2, nDotH);
    const float Vis = VisGeometrySchlickGGX(nDotV, nDotL, roughness);
    const float3 F = FresnelSchlick(vDotH, f0);
    
    const float3 diffuse = DiffuseLambert(albedo);
    const float3 specular = D * Vis * F;
    const float3 lighting = diffuse + specular;
    
    const float shadow = 1.0 - Shadow(light, worldPosition, n, nDotL);

    return lighting * nDotL * light.color * light.intensity * shadow;
}

float3 Light(PointLight light, float3 n, float3 v, float3 worldPosition, float3 albedo, float3 f0, float roughness, float metalness)
{
    const float3 L = light.position - worldPosition;
    
    float distance = length(L);
    float attenuation = 1 - saturate(distance / light.radius);
    attenuation = attenuation * attenuation;
    
    const float3 l = normalize(L);
    const float3 h = normalize(l + v);
    const float nDotL = saturate(dot(n, l));
    const float nDotV = saturate(dot(n, v));
    const float nDotH = saturate(dot(n, h));
    const float vDotH = saturate(dot(v, h));
    
    const float alpha = roughness * roughness;
    const float alpha2 = alpha * alpha;
    
    const float D = NormalDistributionGGX(alpha2, nDotH);
    const float Vis = VisGeometrySchlickGGX(nDotV, nDotL, roughness);
    const float3 F = FresnelSchlick(vDotH, f0);
    
    float3 lighting = DiffuseLambert(albedo) + D * Vis * F;
    return lighting * nDotL * attenuation * light.color * light.intensity;
}