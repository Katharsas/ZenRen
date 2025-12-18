#include "common.hlsl"
#include "worldVertexInput.hlsl"

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

struct VS_OUT
{
    nointerpolation float iTexColor : TEX_INDEX0;
    
    float2 uvTexColor : TEXCOORD0;
    float distance : DISTANCE;
    
    float4 position : SV_POSITION;
};

VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    float4 viewPosition = mul(input.position, worldViewMatrix);
    output.position = mul(viewPosition, projectionMatrix);
    output.distance = length(viewPosition);
    output.uvTexColor = unpackUvTexColor(input);
    output.iTexColor = input.iTexColor;
	return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

Texture2DArray baseColor : register(t0);
SamplerState samplerState : register(s0);

struct PS_IN
{
    nointerpolation float iTexColor : TEX_INDEX0;
    
    float2 uvTexColor : TEXCOORD0;
    float distance : DISTANCE;
};


float4 PS_Main(PS_IN input) : SV_TARGET
{
    float4 diffuseColor = baseColor.Sample(samplerState, float3(input.uvTexColor, input.iTexColor));
    
    diffuseColor = SwitchDiffuseByOutputMode(diffuseColor, (float3) 0.5f, input.distance);
    diffuseColor = ApplyFog(diffuseColor, input.distance);
    
    return diffuseColor;
}