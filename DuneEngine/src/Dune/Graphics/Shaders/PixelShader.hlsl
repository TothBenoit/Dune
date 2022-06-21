
struct PointLight
{
	float3 color;
	float intensity;
	float radius;
	float3 wPos;
};

StructuredBuffer<PointLight> PointLights: register(t0);

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


PS_OUTPUT PSMain(PS_INPUT input)
{
	float3 accumulatedPointLight = { 0,0,0 };
	for (int i = 0; i < 100; i++)
	{
		float3 toLight = PointLights[i].wPos - input.wPos;
		float distToLight = length(toLight);

		float distToLightNorm = 1 - saturate(distToLight/PointLights[i].radius);
		float attenuation = distToLightNorm * distToLightNorm;

		toLight /= distToLight;
		float3 pointLight = PointLights[i].color * saturate(dot(toLight, input.normal.xyz)) * attenuation * PointLights[i].intensity;

		accumulatedPointLight += pointLight;
	}

	float ambientLight = 0.05f;

	PS_OUTPUT output;
	output.color = input.color * float4(saturate(ambientLight + accumulatedPointLight), 1.0f);
	return output;
}