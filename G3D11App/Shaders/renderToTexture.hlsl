//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

struct VS_INPUT
{
	float4 position		: POSITION;
	float2 texcoord		: TEXCOORD0;
};

struct VS_OUTPUT
{
	float2 texcoord	: TEXCOORD;
	float4 position : SV_POSITION;
};

VS_OUTPUT VS_Main(VS_INPUT Input)
{
	VS_OUTPUT Output;
	Output.texcoord = Input.texcoord;
	Output.position = Input.position;
	return Output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

SamplerState SS_Linear : register(s0);
Texture2D TX_SourceBuffer : register(t0);

struct PS_INPUT
{
	float2 texcoord	: TEXCOORD;
};

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
	float4 color = TX_SourceBuffer.Sample(SS_Linear, input.texcoord);
	return color;
}