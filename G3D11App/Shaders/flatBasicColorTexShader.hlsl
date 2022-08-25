//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

cbuffer cbPerObject : register(b0)
{
    float4x4 WVP;
    float baseColorFactor;
};

struct VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float2 uvBaseColor : TEXCOORD0;
};

struct VS_OUT
{
    float2 uvBaseColor : TEXCOORD0;
    float4 normal : NORMAL;
    float4 position : SV_POSITION;
    
};


VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    output.position = mul(input.position, WVP);

    output.normal = mul(input.normal, WVP);


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
    float4 normal : NORMAL;
};

float4 PS_Main(PS_IN input) : SV_TARGET
{
    float4 textureColor = baseColor.Sample(SampleType, input.uvBaseColor);
    float4 color = lerp(float4(1.0f, 1.0f, 1.0f, 0.0f), textureColor, baseColorFactor);

    float4 light = float4(0, -1, 0, 1);
    float4 normal = normalize(input.normal);
    float lightReceivedRatio = max(0, dot(light, normal));

    float shadedColor = lerp(float4(0.0f, 0.0f, 0.0f, 0.0f), color, lightReceivedRatio);

    return shadedColor;
}