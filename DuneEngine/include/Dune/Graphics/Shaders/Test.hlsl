
struct Float4
{
    float4 vValue;
};

ConstantBuffer<Float4> Offset : register(b0);
ConstantBuffer<Float4> Color : register(b0);

struct VS_INPUT
{
	float3 vPos : POSITION;
};

struct VS_OUTPUT
{
    float4 vPosition : SV_Position;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT o;
    o.vPosition = float4(input.vPos, 1.0) + Offset.vValue;
	return o;
}

struct PS_INPUT
{
    float4 vPosition : SV_Position;
};

struct PS_OUTPUT
{
	float4 vColor : SV_TARGET;
};

PS_OUTPUT PSMain(PS_INPUT input)
{
	PS_OUTPUT output;
    output.vColor = Color.vValue;
	return output;
}