#include "ShaderInterop.h"
#include "Common.hlsli"

struct Camera
{
	float4x4 viewProjectionMatrix;
};

ConstantBuffer<Camera> cCamera : register(b0);
ConstantBuffer<InstanceData> cModel : register(b1);

struct VS_INPUT
{
	float3 vPos : POSITION;
#ifdef ALPHA_MASK
	float2 uv   : UV;
#endif
};

struct VS_OUTPUT
{
	float4 position : SV_Position;
#ifdef ALPHA_MASK
	float2 uv       : UV;
#endif
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT o;
	float4 wPos = mul(cModel.objectToWorld, float4(input.vPos, 1.0f));
	o.position = mul(cCamera.viewProjectionMatrix, wPos);
#ifdef ALPHA_MASK
	o.uv = input.uv;
#endif
	return o;
}

#ifdef ALPHA_MASK
ConstantBuffer<MaterialData> cMaterial : register(b1);

void PSMain(VS_OUTPUT input)
{
	float alpha = 1.0f;
	if (IsValid(cMaterial.albedoIdx))
	{
		Texture2D albedoTexture = ResourceDescriptorHeap[cMaterial.albedoIdx];
		alpha *= albedoTexture.Sample(sAnisoWrap, input.uv).a;
	}

	if (alpha < cMaterial.alphaCutoff)
		discard;
}
#endif
