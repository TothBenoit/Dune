
struct PS_INPUT
{
	float4 position : SV_Position;
    float4 color : COLOR0;
	float4 normal : NORMAL;
};

struct PS_OUTPUT
{
	float4 color : SV_TARGET;
};


PS_OUTPUT PSMain(PS_INPUT input)
{
	float3 sunDir = normalize(float3(0.9f, 0.5f, -0.3f));
	float3 sunColor = normalize(float3(120, 192, 224));
	float3 diffuseLight = saturate(dot(input.normal.xyz, sunDir)) * sunColor;
	float ambientLight = 0.2f;

	PS_OUTPUT output;
	output.color = input.color * float4(saturate(ambientLight + diffuseLight), 1.0f);
	return output;
}