#include "ShaderInterop.h"

struct Camera
{
    float4x4 viewProjectionMatrix;
};

ConstantBuffer<Camera> cCamera : register(b0);
ConstantBuffer<InstanceData> cModel : register(b1);

struct VS_INPUT
{
	float3 vPos : POSITION;
};

struct VS_OUTPUT
{
	float4 position : SV_Position;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT o;
    float4 wPos = mul(cModel.modelMatrix, float4(input.vPos, 1.0f));
    o.position = mul(cCamera.viewProjectionMatrix, wPos);
	return o;
}