//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

static const uint OUTPUT_FULL = 0;
static const uint OUTPUT_SOLID = 1;
static const uint OUTPUT_DIFFUSE = 2;
static const uint OUTPUT_NORMAL = 3;
static const uint OUTPUT_LIGHT_SUN = 4;
static const uint OUTPUT_LIGHT_STATIC = 5;

cbuffer cbSettings : register(b0) {
    float4 skyLight;
    
    bool multisampleTransparency;
    bool distantAlphaDensityFix;

    uint outputType;

    float timeOfDay;
    bool skyTexBlur;
};

cbuffer cbPerObject : register(b1)
{
    float4x4 worldViewMatrix;
    float4x4 worldViewMatrixInverseTranposed;
    float4x4 projectionMatrix;
};

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
    float3 uvBaseColor : TEXCOORD0;
    float4 position : SV_POSITION;
};


VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    float4 viewPosition = mul(input.position, worldViewMatrix);
    output.position = mul(viewPosition, projectionMatrix);
    output.uvBaseColor = float3(0.5, 0.5, input.iTexColor);
	return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

Texture2DArray baseColor : register(s0);
Texture2DArray lightmaps : register(s1);
SamplerState SampleType : register(s0);

struct PS_IN
{
    float3 uvBaseColor : TEXCOORD0; // TODO should be called uviTexColor
};


float4 PS_Main(PS_IN input) : SV_TARGET
{
    float4 diffuseColor = baseColor.Sample(SampleType, input.uvBaseColor);
    //clip(diffuseColor.a < 0.4 ? -1 : 1);
    return float4(diffuseColor.rgb, 1);
}