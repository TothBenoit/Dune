struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput VSMain(uint VertexID : SV_VertexID)
{
    VSOutput output;
    output.uv = float2((VertexID << 1) & 2, VertexID & 2);
    output.position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}