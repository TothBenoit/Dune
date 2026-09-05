#include "ShaderInterop.h"
#include "Common.hlsli"

ConstantBuffer<DepthGlobals> cGlobals : register(b0);
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
	o.position = mul(cGlobals.viewProjectionMatrix, wPos);
#ifdef ALPHA_MASK
	o.uv = input.uv;
#endif
	return o;
}

#ifdef ALPHA_MASK
ConstantBuffer<MaterialIndex> cMaterialIndex : register(b1);

void PSMain(VS_OUTPUT input)
{
	float alpha = 1.0f;
	StructuredBuffer<MaterialData> materials = ResourceDescriptorHeap[cGlobals.materialBufferIndex];
	MaterialData material = materials[cMaterialIndex.index];
	if (IsValid(material.albedoIdx))
	{
		Texture2D albedoTexture = ResourceDescriptorHeap[material.albedoIdx];
		alpha *= albedoTexture.Sample(sAnisoWrap, input.uv).a;
	}

	if (alpha < material.alphaCutoff)
		discard;
}
#endif
