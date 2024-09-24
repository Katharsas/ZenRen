//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

cbuffer cbPerObject : register(b1)
{
    float4x4 worldViewMatrix;
    float4x4 worldViewMatrixInverseTranposed;
    float4x4 projectionMatrix;
};

struct VS_IN
{
    float4 position : POSITION;
};

struct VS_OUT
{
    float4 position : SV_POSITION;
};


VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    float4 viewPosition = mul(input.position, worldViewMatrix);
    output.position = mul(viewPosition, projectionMatrix);
    return output;
}