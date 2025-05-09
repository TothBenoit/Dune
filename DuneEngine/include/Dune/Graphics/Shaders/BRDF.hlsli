#pragma once

#include "Common.hlsli"

float NormalDistributionGGX(float alpha2, float nDotH)
{
    float d = (nDotH * alpha2 - nDotH) * nDotH + 1.0f;
    return alpha2 / ( PI * d * d );
}

float VisGeometrySchlickGGX( float nDotV, float nDotL, float roughness )
{
    float k = (roughness + 1.0f);
    k = ( k * k ) * 0.125f;
    return (1.0f / (nDotV * (1.0f - k) + k)) * (1.0f / (nDotL * (1.0f - k) + k)) * 0.25f;
}

float3 ComputeF0( float3 specularColor, float3 albedo, float metalness)
{
    return lerp(specularColor, albedo, metalness);
}

float3 FresnelSchlick( float vDotH, float3 f0 )
{
    float fc = pow(1.0f - vDotH, 5.0f);
    return fc + (1.0f - fc) * f0;
}

float3 DiffuseLambert( float3 albedo )
{
    return albedo * InvPI;
}