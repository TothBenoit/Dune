SamplerState sampleClamp : register(s0);

Texture2D shadowMap[8] : register(t2);

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
};

StructuredBuffer<DirectionalLight> DirectionalLights: register(t1);

struct PS_INPUT
{
	float4 position : SV_Position;
    float4 color : COLOR0;
	float4 normal : NORMAL;
	float4 wPos : WPOS;
};

struct PS_OUTPUT
{
	float4 color : SV_TARGET;
};

float3 AccumulateDirectionalLight(float3 normal)
{
	uint lightCount;
	uint stride;
	DirectionalLights.GetDimensions(lightCount, stride);

	float3 accumulatedDirectionalLight = { 0,0,0 };
	for (int i = 0; i < lightCount; i++)
	{
		float3 toLight = -DirectionalLights[i].dir;
		float3 directionalLight = DirectionalLights[i].color * saturate(dot(toLight, normal)) * DirectionalLights[i].intensity;

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
	float3 accumulatedPointLight = AccumulatePointLight(input.normal.xyz, input.wPos);
	float3 accumulatedDirectionLight = AccumulateDirectionalLight(input.normal.xyz);
	float ambientLight = 0.05f;

	PS_OUTPUT output;
	output.color = input.color * float4(saturate(ambientLight + accumulatedPointLight + accumulatedDirectionLight), 1.0f);
	return output;
}