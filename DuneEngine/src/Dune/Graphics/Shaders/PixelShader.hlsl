SamplerState sampleClamp : register(s0);

Texture2D shadowMap[8] : register(t2);

#define SHADOW_DEPTH_BIAS 0.00005f

struct PointLight
{
	float3 color;
	float intensity;
	float radius;
	float3 wPos;
};

StructuredBuffer<PointLight> PointLights: register(t0);

struct DirectionalLight
{
	float3 color;
	float intensity;
	float3 dir;
	float _padding1;
	float4x4 viewProjMatrix;
};

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

float4 CalcUnshadowedAmountPCF2x2(int lightIndex, float4 vPosWorld)
{
	// Compute pixel position in light space.
	float4 vLightSpacePos = vPosWorld;
	vLightSpacePos = mul(vLightSpacePos, DirectionalLights[lightIndex].viewProjMatrix);

	vLightSpacePos.xyz /= vLightSpacePos.w;

	// Translate from homogeneous coords to texture coords.
	float2 vShadowTexCoord = 0.5f * vLightSpacePos.xy + 0.5f;
	vShadowTexCoord.y = 1.0f - vShadowTexCoord.y;

	// Depth bias to avoid pixel self-shadowing.
	float vLightSpaceDepth = vLightSpacePos.z - SHADOW_DEPTH_BIAS;

	// Find sub-pixel weights.
	float2 vShadowMapDims = float2(1280.0f, 720.0f); // need to keep in sync with .cpp file
	float4 vSubPixelCoords = float4(1.0f, 1.0f, 1.0f, 1.0f);
	vSubPixelCoords.xy = frac(vShadowMapDims * vShadowTexCoord);
	vSubPixelCoords.zw = 1.0f - vSubPixelCoords.xy;
	float4 vBilinearWeights = vSubPixelCoords.zxzx * vSubPixelCoords.wwyy;

	// 2x2 percentage closer filtering.
	float2 vTexelUnits = 1.0f / vShadowMapDims;
	float4 vShadowDepths;
	vShadowDepths.x = shadowMap[0].Sample(sampleClamp, vShadowTexCoord);
	vShadowDepths.y = shadowMap[0].Sample(sampleClamp, vShadowTexCoord + float2(vTexelUnits.x, 0.0f));
	vShadowDepths.z = shadowMap[0].Sample(sampleClamp, vShadowTexCoord + float2(0.0f, vTexelUnits.y));
	vShadowDepths.w = shadowMap[0].Sample(sampleClamp, vShadowTexCoord + vTexelUnits);

	// What weighted fraction of the 4 samples are nearer to the light than this pixel?
	float4 vShadowTests = (vShadowDepths >= vLightSpaceDepth) ? 1.0f : 0.0f;
	return dot(vBilinearWeights, vShadowTests);
}

float3 AccumulateDirectionalLight(float3 normal, float4 wPos)
{
	uint lightCount;
	uint stride;
	DirectionalLights.GetDimensions(lightCount, stride);

	float3 accumulatedDirectionalLight = { 0,0,0 };
	for (int i = 0; i < lightCount; i++)
	{
		float3 toLight = -DirectionalLights[i].dir;
		float3 directionalLight = DirectionalLights[i].color * saturate(dot(toLight, normal)) * DirectionalLights[i].intensity;
		if (i == 0)
		{
			//directionalLight *= CalcUnshadowedAmountPCF2x2(0, wPos);
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

	float3 accumulatedPointLight = { 0,0,0 };
	for (int i = 0; i < lightCount; i++)
	{
		float3 toLight = PointLights[i].wPos - wPos;
		float distToLight = length(toLight);

		float distToLightNorm = 1 - saturate(distToLight / PointLights[i].radius);
		float attenuation = distToLightNorm * distToLightNorm;

		toLight /= distToLight;
		float3 pointLight = PointLights[i].color * saturate(dot(toLight, normal)) * attenuation * PointLights[i].intensity;

		accumulatedPointLight += pointLight;
	}
	return accumulatedPointLight;
}

PS_OUTPUT PSMain(PS_INPUT input)
{
	float3 accumulatedPointLight = AccumulatePointLight(input.normal, input.wPos);
	float3 accumulatedDirectionLight = AccumulateDirectionalLight(input.normal, input.wPos);
	float ambientLight = 0.05f;

	PS_OUTPUT output;
	output.color = input.color * float4(saturate(ambientLight + accumulatedPointLight + accumulatedDirectionLight), 1.0f);
	return output;
}