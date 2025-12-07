#include "ShaderInterop.h"

ConstantBuffer<LuminanceAverageParams> cParams : register(b0);
ByteAddressBuffer bLuminanceHistogram : register(t0);
RWByteAddressBuffer uLuminanceOutput : register(u0);

groupshared float HistogramShared[NUM_HISTOGRAM_BINS];

[numthreads(16, 16, 1)]
void CSMain(uint groupIndex : SV_GroupIndex)
{
    float countForThisBin = float(bLuminanceHistogram.Load(groupIndex << 2));
    HistogramShared[groupIndex] = countForThisBin * (float)groupIndex;
    
    GroupMemoryBarrierWithGroupSync();
    
    [unroll]
    for (uint histogramSampleIndex = (NUM_HISTOGRAM_BINS >> 1); histogramSampleIndex > 0; histogramSampleIndex >>= 1)
    {
        if (groupIndex < histogramSampleIndex)
        {
            HistogramShared[groupIndex] += HistogramShared[groupIndex + histogramSampleIndex];
        }

        GroupMemoryBarrierWithGroupSync();
    }
    
    if (groupIndex == 0)
    {
        float weightedLogAverage = (HistogramShared[0].x / max((float) cParams.pixelCount - countForThisBin, 1.0)) - 1.0;
        float weightedAverageLuminance = exp2(((weightedLogAverage / 255.0) * cParams.logLuminanceRange) + cParams.minLogLuminance);
        float luminanceLastFrame = asfloat(uLuminanceOutput.Load(0));
        float adaptedLuminance = luminanceLastFrame + (weightedAverageLuminance - luminanceLastFrame) * (1 - exp(-cParams.timeDelta * cParams.tau));
        uLuminanceOutput.Store(0, adaptedLuminance);
    }
}