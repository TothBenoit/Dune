
struct PS_INPUT
{
	float4 position : SV_Position;
    float4 color : COLOR0;
};

struct PS_OUTPUT
{
	float4 color : SV_TARGET;
};

PS_OUTPUT PSMain(PS_INPUT input)
{
	PS_OUTPUT output;
	output.color = input.color;
	return output;
}