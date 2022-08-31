//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

static const uint FLAG_OUPUT_DIRECT_SOLID = 0;
static const uint FLAG_OUTPUT_DIRECT_DIFFUSE = 1;
static const uint FLAG_OUTPUT_DIRECT_NORMAL = 2;

cbuffer cbSettings : register(b0) {
    float ambientLight;

    bool outputDirectEnabled;
    uint outputDirectType;
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
    float4 normal : NORMAL;
    float2 uvBaseColor : TEXCOORD0;
};

struct VS_OUT
{
    float2 uvBaseColor : TEXCOORD0;
    float4 color : COLOR;
    float light : LIGHT_INTENSITY;
    //float4 normal : NORMAL;
    float4 position : SV_POSITION;
};


VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output;
    float4 viewPosition = mul(input.position, worldViewMatrix);

    float4 viewNormal = mul(input.normal, worldViewMatrixInverseTranposed);
    float3 viewNormal3 = normalize((float3) viewNormal);

    float4 light = float4(0, 1, 0, 1);
    float4 viewLight = mul(light, worldViewMatrixInverseTranposed);
    float3 viewLight3 = normalize((float3) (viewLight * 1));

    if (!outputDirectEnabled || outputDirectType == FLAG_OUPUT_DIRECT_SOLID) {
        float lightNormalDotProduct = dot(viewNormal3, viewLight3); // for normalized input: range of -1 (down) to 1 (up)
        float lightReceivedRatio = max(0, lightNormalDotProduct) + ambientLight;
        output.light = lightReceivedRatio;
    }
    else {
        output.light = 1;
    }

    output.position = mul(viewPosition, projectionMatrix);
    output.uvBaseColor = input.uvBaseColor;
   
    if (outputDirectType == FLAG_OUTPUT_DIRECT_NORMAL) {
        output.color = float4((float3) input.normal, 1.0f);
    }
    else {
        // vertex color not used by PS
        output.color = float4(1, 1, 1, 1);
    }
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
    float4 color : COLOR;
    float light : LIGHT_INTENSITY;
    //float4 normal : NORMAL;
};

float4 PS_Main(PS_IN input) : SV_TARGET
{
    float4 albedoColor;
    if (!outputDirectEnabled || outputDirectType == FLAG_OUTPUT_DIRECT_DIFFUSE) {
        albedoColor = baseColor.Sample(SampleType, input.uvBaseColor);
    }
    else {
        albedoColor = input.color;
    }
    
    float4 lightColor = float4(float3(1, 1, 1) * input.light, 1);
    float4 shadedColor = albedoColor * lightColor;

    return shadedColor;
}