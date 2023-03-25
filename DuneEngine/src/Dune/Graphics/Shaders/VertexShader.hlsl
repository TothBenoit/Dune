struct CameraConstantBuffer
{
    float4x4 ViewProjMatrix;
};

ConstantBuffer<CameraConstantBuffer> CameraCB : register(b0);

struct InstanceConstantBuffer
{
    float4x4 ModelMatrix;
    float4x4 NormalMatrix;
    float4 BaseColor;
};

StructuredBuffer<InstanceConstantBuffer> InstanceDatas: register(t0);

struct VS_INPUT
{
    float3 vPos : POSITION; 
    float3 vNormal  : NORMAL;
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float4 wPos : WPOS;
};

VS_OUTPUT VSMain(VS_INPUT input, uint instanceID : SV_InstanceID)
{
    VS_OUTPUT o;

    float4 wPos = mul(InstanceDatas[instanceID].ModelMatrix, float4(input.vPos, 1.0f));

    o.wPos = wPos;
    o.position = mul(CameraCB.ViewProjMatrix, wPos);
    o.color = InstanceDatas[instanceID].BaseColor;
    o.normal = normalize(mul(InstanceDatas[instanceID].NormalMatrix, float4(input.vNormal, 1.0f)).xyz);
	return o;
}