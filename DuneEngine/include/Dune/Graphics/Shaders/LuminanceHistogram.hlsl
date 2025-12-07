#include "ShaderInterop.h"
#include "ColorUtils.hlsli"

#define EPSILON 0.0001f

ConstantBuffer<LuminanceHistogramParams> cParams : register(b0);
Texture2D tSource : register(t0);
RWByteAddressBuffer uHistogram : register(u0);

groupshared uint HistogramShared[NUM_HISTOGRAM_BINS];

uint HDRToHistogramBin(float3 hdrColor)
{
    float luminance = Luminance(hdrColor);
    
    if (luminance < EPSILON)
    {
        return 0;
    }
    
    float logLuminance = saturate((log2(luminance) - cParams.minLogLuminance) * cParams.oneOverLogLuminanceRange);
    return (uint) (logLuminance * 254.0f + 1.0f);
}

[numthreads(16, 16, 1)]
void CSMain(uint groupIndex : SV_GroupIndex, uint3 threadId : SV_DispatchThreadID)
{
    HistogramShared[groupIndex] = 0;
    
    GroupMemoryBarrierWithGroupSync();
    
    if (threadId.x < cParams.height && threadId.y < cParams.height)
    {
        float3 hdrColor = tSource.Load(int3(threadId.xy, 0)).rgb;
        uint binIndex = HDRToHistogramBin(hdrColor);
        InterlockedAdd(HistogramShared[binIndex], 1);
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    uHistogram.InterlockedAdd(groupIndex * 4, HistogramShared[groupIndex]);
}