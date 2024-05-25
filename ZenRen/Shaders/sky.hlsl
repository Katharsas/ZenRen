//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

static const uint FLAG_OUTPUT_SOLID = 0;
static const uint FLAG_OUTPUT_DIFFUSE = 1;
static const uint FLAG_OUTPUT_NORMAL = 2;
static const uint FLAG_OUTPUT_LIGHT_SUN = 3;
static const uint FLAG_OUTPUT_LIGHT_STATIC = 4;

cbuffer cbSettings : register(b0) {
    bool multisampleTransparency;
    bool distantAlphaDensityFix;

    bool debugOutput;
    uint debugOutputType;

    float4 skyLight;
    float timeOfDay;
};

cbuffer cbPerObject : register(b1)
{
    float4x4 worldViewMatrix;
    float4x4 worldViewMatrixInverseTranposed;
    float4x4 projectionMatrix;
};

cbuffer cbSkyLayerSettings : register(b2) {
    float texAlphaBase;
    float texAlphaOverlay;
    float2 uvScaleBase;
    float2 uvScaleOverlay;
    float translBase;
    float translOverlay;
    float4 lightOverlay;
};

struct VS_IN
{
    float4 position : POSITION;
//    float4 normal : NORMAL0;
//    float2 uvBaseColor : TEXCOORD0;
//    float3 uvLightmap : TEXCOORD1;
//    float4 colLight : COLOR;
//    float3 dirLight : NORMAL1;
//    float sunLight : TEXCOORD2;
};

struct VS_OUT
{
    float2 uvBaseColor : TEXCOORD0;
    float4 position : SV_POSITION;
};


VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    float4 viewPosition = mul(input.position, worldViewMatrix);
    output.position = mul(viewPosition, projectionMatrix);
    output.position.z = 0.f;// TODO this needs to be 1 in non-reverse-z mode
    output.uvBaseColor = input.position.xz / 50;
    return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

Texture2D skyTexColorBase : register(t0);
Texture2D skyTexColorOverlay : register(t1);
SamplerState SampleType : register(s0);

struct PS_IN
{
    float2 uvBaseColor : TEXCOORD0;
};

float4 PS_Main(PS_IN input) : SV_TARGET
{
    float4 skyBase = skyTexColorBase.Sample(SampleType, input.uvBaseColor);
    float4 skyOverlay = skyTexColorOverlay.Sample(SampleType, input.uvBaseColor);

    // TODO: is ignoring alpha channel of textures correct??
    float4 alphaBase = texAlphaBase;// * skyBase.a;
    float4 alphaOverlay = texAlphaOverlay;// * skyOverlay.a;

    // alphaOver blending algorithm
    float alphaOver = alphaOverlay + alphaBase * (1 - alphaOverlay);
    float3 skyBlend = (skyOverlay.rgb * lightOverlay) * alphaOverlay + (skyBase.rgb) * alphaBase * (1 - alphaOverlay);
    skyBlend = skyBlend / alphaOver;

    float4 diffuseColor = float4(skyBlend * 0.7, alphaOver);

    float4 shadedColor = diffuseColor;
    return shadedColor;
}