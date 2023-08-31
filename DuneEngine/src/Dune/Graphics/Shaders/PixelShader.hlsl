#include "DirectionalLight.h"
#include "PointLight.h"

SamplerState sampleClamp : register(s0);

Texture2D shadowMap[1] : register(t2);

#define SHADOW_DEPTH_BIAS 0.0005f

struct CameraConstantBuffer
{
	float4x4 ViewProjMatrix;
	float4 cameraWorldPos;
};


ConstantBuffer<CameraConstantBuffer> CameraCB : register(b0);

StructuredBuffer<PointLight> PointLights: register(t0);

StructuredBuffer<DirectionalLight> DirectionalLights: register(t1);

struct PS_INPUT
{
	float4 position : SV_Position;
    float4 color : COLOR0;
	float3 normal : NORMAL;
	float4 wPos : WPOS;
};

struct PS_OUTPUT
{
	float4 color : SV_TARGET;
};

float4 CalcUnshadowedAmountPCF2x2(int lightIndex, float3 vPosWorld, float3 normal, float nDotL)
{
	// Compute pixel position in light space.
	float4 vLightSpacePos = float4(vPosWorld + (normal.xyz * (1.0f - abs(nDotL))),1.f);
	vLightSpacePos = mul(DirectionalLights[lightIndex].m_viewProj, vLightSpacePos);

	vLightSpacePos.xyz /= vLightSpacePos.w;

	// Translate from homogeneous coords to texture coords.
	float2 vShadowTexCoord = 0.5f * vLightSpacePos.xy + 0.5f;
	vShadowTexCoord.y = 1.0f - vShadowTexCoord.y;

	// Depth bias to avoid pixel self-shadowing.
	float vLightSpaceDepth = vLightSpacePos.z - SHADOW_DEPTH_BIAS;

	// Find sub-pixel weights.
	float2 vShadowMapDims = float2(8192.f, 8192.f); // need to keep in sync with .cpp file
	float4 vSubPixelCoords = float4(1.0f, 1.0f, 1.0f, 1.0f);
	vSubPixelCoords.xy = frac(vShadowMapDims * vShadowTexCoord);
	vSubPixelCoords.zw = 1.0f - vSubPixelCoords.xy;
	float4 vBilinearWeights = vSubPixelCoords.zxzx * vSubPixelCoords.wwyy;

	float2 vTexelUnits = 1.0f / vShadowMapDims;
	// 2x2 percentage closer filtering.
	float4 vShadowDepths;
	vShadowDepths.x = shadowMap[lightIndex].Sample(sampleClamp, vShadowTexCoord).x;
	vShadowDepths.y = shadowMap[lightIndex].Sample(sampleClamp, vShadowTexCoord + float2(vTexelUnits.x, 0.0f)).x;
	vShadowDepths.z = shadowMap[lightIndex].Sample(sampleClamp, vShadowTexCoord + float2(0.0f, vTexelUnits.y)).x;
	vShadowDepths.w = shadowMap[lightIndex].Sample(sampleClamp, vShadowTexCoord + vTexelUnits).x;

	// What weighted fraction of the 4 samples are nearer to the light than this pixel?
	float4 vShadowTests = (vShadowDepths >= vLightSpaceDepth) ? 1.0f : 0.0f;
	return dot(vBilinearWeights, vShadowTests);
}

float3 AccumulateDirectionalLight(float3 normal, float4 wPos)
{
	uint lightCount;
	uint stride;
	DirectionalLights.GetDimensions(lightCount, stride);

	float3 cameraDir = normalize(CameraCB.cameraWorldPos.xyz - wPos);

	float3 accumulatedDirectionalLight = { 0,0,0 };
	for (uint i = 0; i < lightCount; i++)
	{
		float3 toLight = -DirectionalLights[i].m_dir;

		float diffuse = max(dot(toLight, normal), 0.f);
		float3 reflection = reflect(DirectionalLights[i].m_dir, normal);
		float specular = pow(max(dot(cameraDir, reflection), 0.f), 8.f);

		float3 directionalLight = (diffuse + specular) * DirectionalLights[i].m_color * DirectionalLights[i].m_intensity;

		if (i == 0)
		{
			directionalLight *= CalcUnshadowedAmountPCF2x2(0, wPos.xyz, normal, diffuse).xyz;
		}

		accumulatedDirectionalLight += directionalLight;
	}
	return accumulatedDirectionalLight;
}

float3 AccumulatePointLight(float3 normal, float3 wPos)
{
	uint lightCount;
	uint stride;
	PointLights.GetDimensions(lightCount, stride);

	float3 cameraDir = normalize(CameraCB.cameraWorldPos.xyz - wPos);

	float3 accumulatedPointLight = { 0.f, 0.f, 0.f };
	for (uint i = 0; i < lightCount; i++)
	{
		float3 toLight = PointLights[i].m_pos - wPos;
		float distToLight = length(toLight);
		toLight /= distToLight;

		float diffuse = max(dot(toLight, normal), 0.f);
		float3 reflection = reflect(-toLight, normal);
		float specular = pow(max(dot(cameraDir, reflection), 0.f), 8.f);

		float distToLightNorm = 1 - saturate(distToLight / PointLights[i].m_radius);
		float attenuation = distToLightNorm * distToLightNorm;

		float3 pointLight = (diffuse + specular) * PointLights[i].m_color * attenuation * PointLights[i].m_intensity;

		accumulatedPointLight += pointLight;
	}

	return accumulatedPointLight;
}

PS_OUTPUT PSMain(PS_INPUT input)
{
	float3 accumulatedPointLight = AccumulatePointLight(input.normal, input.wPos);
	float3 accumulatedDirectionLight = AccumulateDirectionalLight(input.normal, input.wPos);
	float3 ambientLight = 0.05f;

	PS_OUTPUT output;
	output.color = input.color * saturate(float4(ambientLight + accumulatedPointLight + accumulatedDirectionLight, 1.0f));
	return output;
}