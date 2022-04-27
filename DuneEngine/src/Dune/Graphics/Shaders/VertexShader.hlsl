struct VS_INPUT
{
    float3 vPos : POSITION;
    float4 vColor : COLOR;
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float4 color : COLOR;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT o;
    o.position = float4(input.vPos, 1.0f);
    o.color = input.vColor;

	return o;
}