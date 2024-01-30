//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

static const uint FLAG_OUPUT_DIRECT_SOLID = 0;
static const uint FLAG_OUTPUT_DIRECT_DIFFUSE = 1;
static const uint FLAG_OUTPUT_DIRECT_NORMAL = 2;
static const uint FLAG_OUTPUT_DIRECT_LIGHTMAP = 3;

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
    float3 uvLightmap : TEXCOORD1;
    //float4 colorLightmap : COLOR;
};

struct VS_OUT
{
    float2 uvBaseColor : TEXCOORD0;
    float3 uvLightmap : TEXCOORD1;
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

    if (!outputDirectEnabled || outputDirectType == FLAG_OUPUT_DIRECT_SOLID || outputDirectType == FLAG_OUTPUT_DIRECT_LIGHTMAP) {
        float lightNormalDotProduct = dot(viewNormal3, viewLight3); // for normalized input: range of -1 (down) to 1 (up)
        float lightReceivedRatio = max(0, lightNormalDotProduct + 0.3f) + ambientLight;
        output.light = lightReceivedRatio;
    }
    else {
        output.light = 1;
    }

    output.position = mul(viewPosition, projectionMatrix);
    output.uvBaseColor = input.uvBaseColor;
    output.uvLightmap = input.uvLightmap;
   
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
Texture2DArray lightmaps : register(s1);
SamplerState SampleType : register(s0);

struct PS_IN
{
    float2 uvBaseColor : TEXCOORD0;
    float3 uvLightmap : TEXCOORD1;
    float4 color : COLOR;
    float light : LIGHT_INTENSITY;
    //int indexLightmap : INDEX_LIGHTMAP;
    //float4 normal : NORMAL;
};

float4 PS_Main(PS_IN input) : SV_TARGET
{
    float4 albedoColor;
    if (!outputDirectEnabled || outputDirectType == FLAG_OUTPUT_DIRECT_DIFFUSE) {
        albedoColor = baseColor.Sample(SampleType, input.uvBaseColor);

        // Alpha Cutoff
        // lower values will result in thinner coverage
        float alphaCutoff = 0.6;

        // Alpha to Coverage Sharpening
        // sharpen the multisampled alpha to remove banding and interior transparency; outputs close to 1 or 0
        albedoColor.a = (albedoColor.a - (1 - alphaCutoff)) / max(fwidth(albedoColor.a), 0.0001) + 0.5;

        // Alpha Test
        // if sharpening is used, the alphaCutoff its pretty much irrelevant here (we could just use 0.5)
        clip(albedoColor.a < alphaCutoff ? -1 : 1);
    }
    else if (outputDirectEnabled && outputDirectType == FLAG_OUTPUT_DIRECT_LIGHTMAP) {
        // this splits pixels into per face groups based on lightmap or not, which is not great (?)
        if (input.uvLightmap.z >= 0) {
            albedoColor = lightmaps.Sample(SampleType, input.uvLightmap);
        }
        else {
            albedoColor = float4(1, 1, 1, 1);
        }
    }
    else {
        albedoColor = input.color;
    }
    
    float4 lightColor = float4(float3(1, 1, 1) * input.light, 1);
    float4 shadedColor = albedoColor * lightColor;

    return shadedColor;
}