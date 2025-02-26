#include "common.hlsl"

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

struct VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL0;
    float2 uvBaseColor : TEXCOORD0; // TODO should be called uvTexColor
    float3 uvLightmap : TEXCOORD1; // TODO should be called uviTexLightmap
    float4 colLight : COLOR;
    float3 dirLight : NORMAL1;
    float sunLight : TEXCOORD2;
    uint1 iTexColor : TEXCOORD3;
};

struct VS_OUT
{
    nointerpolation float iTexColor : TEX_INDEX0;
    
    float2 uvTexColor : TEXCOORD0;
    float4 position : SV_POSITION;
};


VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    float4 viewPosition = mul(input.position, worldViewMatrix);
    output.position = mul(viewPosition, projectionMatrix);
    output.uvTexColor = input.uvBaseColor;
    output.iTexColor = input.iTexColor;
	return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

Texture2DArray baseColor : register(s0);
SamplerState SampleType : register(s0);

struct PS_IN
{
    nointerpolation float iTexColor : TEX_INDEX0;
    
    float2 uvTexColor : TEXCOORD0;
};


float4 PS_Main(PS_IN input) : SV_TARGET
{
    float4 diffuseColor = baseColor.Sample(SampleType, float3(input.uvTexColor, input.iTexColor));
    //clip(diffuseColor.a < 0.4 ? -1 : 1);
    return diffuseColor;
}