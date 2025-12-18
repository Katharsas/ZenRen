#include "common.hlsl"

cbuffer cbSkyLayerSettings : register(b4)
{
    float4 colBackground;
    
    struct SkyLayer
    {
        float4 light;
        float alpha;
        bool blurDisabled;
    }
    texLayers[2];
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

struct VS_IN
{
    float4 position : POSITION;
    float2 uvBase : TEXCOORD0;
    float2 uvOverlay : TEXCOORD1;
};

struct VS_OUT
{
    float2 uvBase : TEXCOORD0;
    float2 uvOverlay : TEXCOORD1;
    
    float4 worldPos : POSITION;
    float4 position : SV_POSITION;
};


VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    output.worldPos = input.position;
    float4 viewPosition = mul(input.position, worldViewMatrix);
    output.position = mul(viewPosition, projectionMatrix);
    output.position.z = reverseZ ? 0 : 1 * output.position.w;
    output.uvBase = input.uvBase;
    output.uvOverlay = input.uvOverlay;
    return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

Texture2D skyTexColor[2] : register(t0);
SamplerState samplerState : register(s0);

struct PS_IN
{
    float2 uvBase : TEXCOORD0;
    float2 uvOverlay : TEXCOORD1;
    
    float4 worldPos : POSITION;
    float4 position : SV_POSITION;
};

float4 FastGaussianBlur(Texture2D<float4> sourceTex, float2 uv)
{
    float Pi = 6.28318530718; // Pi*2

    // settings
    float Directions = 7; // BLUR DIRECTIONS (Default 16.0 - More is better but slower)
    float Quality = 3.0; // BLUR QUALITY (Default 4.0 - More is better but slower)
    float Size = 2; // BLUR SIZE (Default 4.0 - Radius)

    float2 radius = Size / (float2) 500;// Size was meant be relative to tex resolution
    float4 color = sourceTex.Sample(samplerState, uv);

    // blur
    for (float d = 0.0; d < Pi; d += Pi / Directions)
    {
        for (float i = 1.0 / Quality; i <= 1.0; i += 1.0 / Quality)
        {
            color += sourceTex.Sample(samplerState, uv + float2(cos(d), sin(d)) * radius * i);
        }
    }

	color /= Quality * Directions - 4;// constant must be manually adjusted to get same brightness
    return color;
}

float3 AlphaOverToRgb(float3 base, float3 over, float overAlpha)
{
    return over * overAlpha + base * (1 - overAlpha);
}

float4 AlphaOverToRgba(float3 base, float baseAlpha, float3 over, float overAlpha)
{
    float alpha = overAlpha + baseAlpha * (1 - overAlpha);
    float3 color = over * overAlpha + base * baseAlpha * (1 - overAlpha);
    color /= alpha;
    return float4(color, alpha);
}

float4 SkyTexColor(int layer, float2 uv)
{
    // TODO refactor this stuff to be similar to main or even share functions
    
    float4 color;
    if (skyTexBlur && !texLayers[layer].blurDisabled) {
        // blur layer textures (including alpha) to conceal banding
        color = FastGaussianBlur(skyTexColor[layer], uv);
    }
    else {
        color = skyTexColor[layer].Sample(samplerState, uv);
    }
    float3 light = texLayers[layer].light.rgb;
    
    // debug modes
    if (!(outputType == OUTPUT_FULL || outputType == OUTPUT_DIFFUSE)) {
        color.rgb = (float3) 1;
    }
    if (!(outputType == OUTPUT_FULL || outputType == OUTPUT_SOLID)) {
        light = (float3) (outputType == OUTPUT_DIFFUSE ? 1 : 0);
    }

	color.rgb *= light;
	return color;
}

float4 PS_Main(PS_IN input) : SV_TARGET
{
	float4 skyBase = SkyTexColor(0, input.uvBase);
	float4 skyOverlay = SkyTexColor(1, input.uvOverlay);

    // blend overlay over base
	float alphaBase = texLayers[0].alpha * skyBase.a;
	float alphaOverlay = texLayers[1].alpha * skyOverlay.a;
	float4 skyBlend = AlphaOverToRgba(skyBase.rgb, alphaBase, skyOverlay.rgb, alphaOverlay);

    // adjust colors and blend sky over background
	float4 shadedColor = float4(AlphaOverToRgb(colBackground.rgb, skyBlend.rgb, skyBlend.a), 1);
    
    // fog
    float centerDist = length(input.worldPos.xz);
    shadedColor = ApplyFog(shadedColor, centerDist * distanceFogSkyFactor, true);
    
	return shadedColor;
}