#include "ShaderTypes.h"
#include "BRDF.hlsli"

float3 Light(DirectionalLight light, float3 n, float3 v, float3 albedo, float roughness, float metalness)
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
    const float3 F = FresnelSchlick(vDotH, ComputeF0(0.04f.xxx, albedo, metalness));
    
    float3 lighting = DiffuseLambert(albedo) + D * Vis * F;
    return lighting * nDotL * light.color * light.intensity;
}

float3 Light(PointLight light, float3 n, float3 v, float3 worldPosition, float3 albedo, float roughness, float metalness)
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
    const float3 F = FresnelSchlick(vDotH, ComputeF0(0.04f.xxx, albedo, metalness));
    
    float3 lighting = DiffuseLambert(albedo) + D * Vis * F;
    return lighting * nDotL * attenuation * light.color * light.intensity;
}