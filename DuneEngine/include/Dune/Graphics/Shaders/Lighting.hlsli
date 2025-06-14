#include "ShaderTypes.h"
#include "BRDF.hlsli"

float3 GetFaceDirection(const float3 v)
{
    float3 vAbs = abs(v);
    float3 localDirection = 0;
    if (vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)
    {
        localDirection = v.z < 0 ? float3(-v.x, v.y, -v.z) : float3(v.x, v.y, v.z);
    }
    else if (vAbs.y >= vAbs.x)
    {
        localDirection = v.y < 0 ? float3(-v.x, -v.z, -v.y) : float3(v.x, -v.z, v.y);
    }
    else
    {
        localDirection = v.x < 0 ? float3(-v.z, v.y, -v.x) : float3(v.z, v.y, v.x);
    }
    return localDirection;
}

float Shadow(Light light, uint lightMatricesIndex, float3 worldPosition, float3 n, float nDotL)
{
    StructuredBuffer<float4x4> lightMatrices = ResourceDescriptorHeap[lightMatricesIndex];
    float4x4 lightMatrix = lightMatrices[light.matrixIndex];
    
    if (light.IsPoint())
    {
        const float3 lightToWorld = worldPosition - light.position;
        const float3 uv = normalize(lightToWorld);
        const float3 localDirection = GetFaceDirection(lightToWorld);
        float4 lightPos = mul(lightMatrix, float4(localDirection, 1.f));
        lightPos.xyz /= lightPos.w;
        TextureCube shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowIndex)];
        return shadowMap.SampleCmpLevelZero(sLinearClampComparisonGreater, uv, lightPos.z);
    }

    float4 lightPos = mul(lightMatrix, float4(worldPosition, 1.f));
    lightPos.xyz /= lightPos.w;
    const float2 uv = lightPos.xy * float2(0.5f, -0.5f) + 0.5f;
    Texture2D shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowIndex)];
    return shadowMap.SampleCmpLevelZero(sLinearClampComparisonGreater, uv, lightPos.z);
}

float RangeAttenuation(float distanceSq, float range)
{
    const float distanceAttenuation = rcp(distanceSq + 1.0);
    const float window = Square(saturate(1.0 - Square(distanceSq * Square(rcp(range)))));
    return distanceAttenuation * window;
}

float3 ComputeLight(Light light, uint lightMatricesIndex, float3 n, float3 v, float3 worldPosition, float3 diffuseColor, float3 f0, float roughness)
{
    float3 l = -light.direction;
    float attenuation = 1.f;
    
    if (light.IsPoint() || light.IsSpot())
    {
        float3 L = light.position - worldPosition;
        const float distanceSq = dot(L, L);
        attenuation *= RangeAttenuation(distanceSq, light.range);
        
        l = L * rsqrt(distanceSq);
        
        if (light.IsSpot())
        {
            const float cosAngle = dot(light.direction, -l);
            attenuation *= Square(saturate((cosAngle - light.angle) * light.penumbra));
        }
    }

    if (light.HasShadow())
    {
        const float nDotL = saturate(dot(n, l));
        attenuation *= 1.0f - Shadow(light, lightMatricesIndex, worldPosition, n, nDotL);
    }

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