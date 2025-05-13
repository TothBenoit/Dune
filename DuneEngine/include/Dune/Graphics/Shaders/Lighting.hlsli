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

float RadialAttenuation(float3 L, float range)
{
    const float distanceSq = dot(L, L);
    const float distanceAttenuation = rcp(distanceSq + 1.0);
    const float window = Square(saturate(1.0 - Square(distanceSq * Square(rcp(range)))));
    return distanceAttenuation * window;
}

float3 Light(DirectionalLight light, float3 n, float3 v, float3 worldPosition, float3 diffuseColor, float3 f0, float roughness)
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
    const float3 specular = D * Vis * F;
   
    const float shadow = 1.0 - Shadow(light, worldPosition, n, nDotL);
    const float3 lightColor = shadow * light.color * light.intensity;

    const float3 BRDF = DiffuseLambert(diffuseColor) + specular;
    return BRDF * lightColor * nDotL;
}

float3 Light(PointLight light, float3 n, float3 v, float3 worldPosition, float3 diffuseColor, float3 f0, float roughness)
{
    const float3 L = light.position - worldPosition;
    const float attenuation = RadialAttenuation(L, light.radius);
    
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
    const float3 specular = D * Vis * F;
    
    const float3 lightColor = attenuation * light.color * light.intensity;
    
    const float3 BRDF = DiffuseLambert(diffuseColor) + specular;
    return BRDF * lightColor * nDotL;
}

float3 Light(SpotLight light, float3 n, float3 v, float3 worldPosition, float3 diffuseColor, float3 f0, float roughness)
{
    const float3 L = light.position - worldPosition;
    const float rangeAttenuation = RadialAttenuation(L, light.range);
    
    const float3 l = normalize(L);
    const float cosAngle = dot(light.direction, -l);
    const float angleAttenuation = Square(saturate((cosAngle - light.angle) * light.penumbra));
    
    const float attenuation = rangeAttenuation * angleAttenuation;
    
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
    const float3 specular = D * Vis * F;
    
    const float3 lightColor = attenuation * light.color * light.intensity;
    
    const float3 BRDF = DiffuseLambert(diffuseColor) + specular;
    return BRDF * lightColor * nDotL;
}