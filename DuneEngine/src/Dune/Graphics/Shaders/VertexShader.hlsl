struct CameraConstantBuffer
{
    float4x4 ViewProjMatrix;
};

ConstantBuffer<CameraConstantBuffer> CameraCB : register(b1);

struct InstanceConstantBuffer
{
    float4x4 ModelMatrix;
    float4x4 NormalMatrix;
    float4 BaseColor;
};

ConstantBuffer<InstanceConstantBuffer> InstanceCB: register(b0);

struct VS_INPUT
{
    float3 vPos : POSITION;
    float4 vColor : COLOR;
    float3 vNormal  : NORMAL;
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float4 color : COLOR;
    float4 normal : NORMAL;
    float4 wPos : WPOS;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT o;

    float4 wPos = mul(InstanceCB.ModelMatrix, float4(input.vPos, 1.0f));

    o.wPos = wPos;
    o.position = mul(CameraCB.ViewProjMatrix, wPos);
    o.color = InstanceCB.BaseColor;
    o.normal = mul(InstanceCB.NormalMatrix, float4(input.vNormal, 1.0f));
	return o;
}