#pragma once

SamplerState sLinearWrap                                : register(s0,    space1);
SamplerState sLinearClamp                               : register(s1,    space1);
SamplerState sLinearBorder                              : register(s2,    space1);
SamplerState sPointWrap                                 : register(s3,    space1);
SamplerState sPointClamp                                : register(s4,    space1);
SamplerState sPointBorder                               : register(s5,    space1);
SamplerState sAnisoWrap                                 : register(s6,    space1);
SamplerState sAnisoClamp                                : register(s7,    space1);
SamplerState sAnisoBorder                               : register(s8,    space1);
SamplerComparisonState sLinearClampComparisonGreater    : register(s9,    space1);
SamplerComparisonState sLinearWrapComparisonGreater     : register(s10,   space1);

static const float PI = 3.14159265358979323846f;
static const float InvPI = 0.31830988618379067154f;

float3x3 TangentToWorld(float3 normal, float4 tangent)
{
    float3 T = tangent.xyz;
    float3 B = cross(normal, T) * tangent.w;
    float3x3 TBN = float3x3(T, B, normal);
    return TBN;
}

float3 UnpackNormal(float2 packedNormal)
{
    const float2 normalXY = packedNormal * 2.0f - 1.0f;
    const float normalZ = sqrt(1 - saturate(dot(packedNormal.xy, packedNormal.xy)));
    return float3(normalXY, normalZ);
}

template<typename T>
T Square(T x)
{
    return x * x;
}