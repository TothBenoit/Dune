#include "ShaderTypes.h"
#include "Common.hlsli"

Texture2D tSource : register(t0);

float3 FilmicTonemapping(float3 color )
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return (color * (a * color + b)) / (color * (c * color + d) + e);
}

float3 GammaCorrection(float3 color)
{
    return pow(color, 1.0f / 2.2f);
}

void PSMain(float4 position : SV_Position, float2 uv : TEXCOORD, out float4 color : SV_TARGET)
{
    const float3 sceneColor = tSource.Sample(sPointClamp, uv).xyz;
    color.xyz = GammaCorrection(FilmicTonemapping(sceneColor));
    color.w = 1.0f;
}