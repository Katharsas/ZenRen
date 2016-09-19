//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

struct VS_INPUT
{
	float4 position		: POSITION;
	float2 vTex1		: TEXCOORD0;
};

struct VS_OUTPUT
{
	float2 texcoord	: TEXCOORD;
	float4 position : SV_POSITION;
};


VS_OUTPUT VS_Main(VS_INPUT Input)
{
	VS_OUTPUT Output;

	Output.texcoord = Input.vTex1;
	Output.position = Input.position;

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

