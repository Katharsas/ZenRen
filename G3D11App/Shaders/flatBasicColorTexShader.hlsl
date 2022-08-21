//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

cbuffer cbPerObject
{
    float4x4 WVP;
};

struct VS_IN
{
    float4 position : POSITION;
    float2 uvBaseColor : TEXCOORD0;
};

struct VS_OUT
{
    float2 uvBaseColor : TEXCOORD0;
    float4 position : SV_POSITION;
};


VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    output.position = mul(input.position, WVP);
    output.uvBaseColor = input.uvBaseColor;
	return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

Texture2D baseColor : register(s0);
SamplerState SampleType : register(s0);

struct PS_IN
{
    float2 uvBaseColor : TEXCOORD0;
};

float4 PS_Main(PS_IN input) : SV_TARGET
{
    float4 textureColor = baseColor.Sample(SampleType, input.uvBaseColor);
    return textureColor;
}