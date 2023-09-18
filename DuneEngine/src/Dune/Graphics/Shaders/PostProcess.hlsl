#include "PostProcessGlobals.h"

ConstantBuffer<PostProcessGlobals> globals : register(b0);

Texture2D sceneColor : register(t0);
Texture2D sceneDepth : register(t1);
SamplerState sceneSampler : register(s0);

struct VSOutput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD;
};

VSOutput VSMain(uint VertexID : SV_VertexID)
{
	VSOutput output;
	output.uv = float2((VertexID << 1) & 2, VertexID & 2);
	output.position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0, 1);
	return output;
}

struct PSInput
{
	float4 position: SV_Position;
	float2 uv : TEXCOORD;
};

float4 PSMain(PSInput input) : SV_Target
{
    const float threshold = 0.99f;
    float2 texelSize = float2(1.f / globals.m_screenResolution);
    
    const float2 texAddrOffsets[8] =
    {
        float2(-1, -1),
        float2( 0, -1),
        float2( 1, -1),
        float2(-1,  0),
        float2( 1,  0),
        float2(-1,  1),
        float2( 0,  1),
        float2( 1,  1),
    };

    float3 LuminanceConv = { 0.2125f, 0.7154f, 0.0721f };
    float lum[8];
    int i;

    for (i = 0; i < 8; i++)
    {
        float4 ndcCoords = float4(0, 0, sceneDepth.Sample(sceneSampler, input.uv + (texAddrOffsets[i] * texelSize)).r, 1.0f);
        float4 viewCoords = mul(globals.m_invProj, ndcCoords);
        float linearDepth = viewCoords.z / viewCoords.w;
        lum[i] = dot(linearDepth.rrr, LuminanceConv);
    }

    float x = lum[0] + 2 * lum[3] + lum[5] - lum[2] - 2 * lum[4] - lum[7];
    float y = lum[0] + 2 * lum[1] + lum[2] - lum[5] - 2 * lum[6] - lum[7];
    float edge = step(threshold, sqrt(x * x + y * y));

    float4 color = sceneColor.Sample(sceneSampler, input.uv);

    return lerp(color, float4(1.f, 0.5f, 0.0f, 1.f), edge);
}