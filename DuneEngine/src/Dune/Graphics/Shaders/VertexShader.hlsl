struct CameraMatrix
{
    float4x4 ViewProjMatrix;
};

ConstantBuffer< CameraMatrix> CameraMatrixCB : register(b2);

struct InstanceMatrices
{
    float4x4 ModelMatrix;
    float4x4 NormalMatrix;
};

ConstantBuffer<InstanceMatrices> InstanceMatricesCB: register(b0);

struct Material
{
    float4 BaseColor;
};

ConstantBuffer<Material> MaterialCB : register(b1);

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
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT o;
    float4x4 MVP = mul(CameraMatrixCB.ViewProjMatrix, InstanceMatricesCB.ModelMatrix);
    o.position = mul(MVP, float4(input.vPos, 1.0f) );
    o.color = MaterialCB.BaseColor;
    o.normal = mul(InstanceMatricesCB.NormalMatrix, float4(input.vNormal, 1.0f));

	return o;
}