//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

struct VS_INPUT
{
	float3 vPosition	: POSITION;
	float2 vTex1		: TEXCOORD0;
	uint vertexID		: SV_VertexID;
};

struct VS_OUTPUT
{
	float2 vTexcoord	: TEXCOORD0;
};


VS_OUTPUT VS_Main(VS_INPUT Input)
{
	VS_OUTPUT Output;

	Output.vTexcoord = Input.vTex1;

	return Output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

SamplerState SS_Linear : register(s0);
Texture2D TX_BackBufferHDR : register(t0);

struct PS_INPUT
{
	float2 vTexcoord	: TEXCOORD;
};

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
	float4 color = TX_BackBufferHDR.Sample(SS_Linear, input.vTexcoord);
	// TODO: Gamma/ToneMapping
	return color;
}

