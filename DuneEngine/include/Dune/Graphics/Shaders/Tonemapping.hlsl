#include "ShaderInterop.h"
#include "Common.hlsli"
#include "ColorUtils.hlsli"

Texture2D tSource : register(t0);
ByteAddressBuffer bLuminanceAverage : register(t1);

float3 FilmicTonemapping(float3 color )
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return (color * (a * color + b)) / (color * (c * color + d) + e);
}

float EV100FromLuminance(float luminance)
{
    const float K = 12.5f;
    const float ISO = 100.0f;
    return log2(luminance * (ISO / K));
}

float Exposure(float ev100)
{
    return 1.0f / (pow(2.0f, ev100) * 1.2f);
}

void PSMain(float4 position : SV_Position, float2 uv : TEXCOORD, out float4 color : SV_TARGET)
{
    float3 hdrColor = tSource.Sample(sPointClamp, uv).xyz;
    
    float averageLuminance = asfloat(bLuminanceAverage.Load(0));
    float exposure = Exposure(EV100FromLuminance(averageLuminance));
    hdrColor = hdrColor * (exposure + 1);
    
    color.xyz = LinearToSRGB(FilmicTonemapping(hdrColor));
    color.w = 1.0f;
}