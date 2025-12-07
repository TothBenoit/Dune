#pragma once

float3 LinearToSRGB( float3 linearColor )
{
    float3 sRGBLo = linearColor * 12.92f;
    float3 sRGBHi = 1.055f * pow(linearColor, 0.41666f) - 0.055f;
    return select(linearColor <= 0.0031308f, sRGBLo, sRGBHi);
}

float3 SRGBToLinear( float3 sRGBColor )
{
    float3 linearLo = sRGBColor * 0.0773993808f;
    float3 linearHi = pow(sRGBColor * 0.9478672986f + 0.0521327014f, 2.4f);
    return select(sRGBColor <= 0.04045f, linearLo, linearHi);
}

float Luminance( float3 linearColor )
{
    return dot(linearColor, float3(0.2126729, 0.7151522, 0.0721750));
}