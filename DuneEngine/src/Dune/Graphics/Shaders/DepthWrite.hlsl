#include "DirectionalLight.h"
#include "PointLight.h"

#define SHADOW_MAP_COUNT 4
#define SHADOW_DEPTH_BIAS 0.0005f

struct CameraConstantBuffer
{
	float4x4 ViewProjMatrix;
};

struct InstanceConstantBuffer
{
	float4x4 ModelMatrix;
	float4x4 NormalMatrix;
	float4 BaseColor;
};

ConstantBuffer<CameraConstantBuffer> CameraCB : register(b0);
StructuredBuffer<InstanceConstantBuffer> InstanceDatas : register(t0);

struct VS_INPUT
{
	float3 vPos : POSITION;
	float3 vNormal  : NORMAL;
};

struct VS_OUTPUT
{
	float4 position : SV_Position;
};

VS_OUTPUT VSMain(VS_INPUT input, uint instanceID : SV_InstanceID)
{
	VS_OUTPUT o;
	float4 wPos = mul(InstanceDatas[instanceID].ModelMatrix, float4(input.vPos, 1.0f));
	o.position = mul(CameraCB.ViewProjMatrix, wPos);
	return o;
}