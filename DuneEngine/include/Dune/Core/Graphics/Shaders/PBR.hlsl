#include "PBR.h"

ConstantBuffer<PBRGlobals> globals : register(b0);
ConstantBuffer<PBRInstance> instance : register(b1);

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
};


VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT o;
    
    float4 wPos = mul(instance.modelMatrix, float4(input.position, 1.0f));
    o.position = mul(globals.viewProjectionMatrix, wPos);
    o.normal = normalize(mul((float3x3)instance.modelMatrix, input.normal));
    return o;
}

ConstantBuffer<PBRMaterial> material : register(b1);

struct PS_INPUT
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
};

struct PS_OUTPUT
{
    float4 color : SV_TARGET;
};

PS_OUTPUT PSMain(PS_INPUT input)
{
    PS_OUTPUT output;
    output.color = float4(material.albedo * saturate(dot(input.normal, -globals.sunDirection) + 0.05f), 1.0f);
    return output;
}