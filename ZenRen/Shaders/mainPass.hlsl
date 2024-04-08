//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

static const uint FLAG_OUTPUT_DIRECT_SOLID = 0;
static const uint FLAG_OUTPUT_DIRECT_DIFFUSE = 1;
static const uint FLAG_OUTPUT_DIRECT_NORMAL = 2;
static const uint FLAG_OUTPUT_DIRECT_LIGHT_STATIC = 3;
static const uint FLAG_OUTPUT_DIRECT_LIGHTMAP = 4;

cbuffer cbSettings : register(b0) {
    bool multisampleTransparency;
    bool distantAlphaDensityFix;

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
    float4 colLight : COLOR;
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

    // TODO doing light calculation in view space results in flickering,
    // so until we know whats going wrong just do it in world space.

    float3 viewNormal3 = normalize((float3) input.normal);
    //float3 viewNormal3 = normalize((float3) mul(input.normal, worldViewMatrixInverseTranposed));
    //float3 viewNormal3 = normalize(mul((float3) input.normal, (float3x3) worldViewMatrixInverseTranposed));

    // light dir is really a normal pointing directly towards light
    float3 viewLight3 = float3(0, 1, 0);
    //float3 viewLight3 = normalize((float3) mul(float4(0, 1, 0, 0), worldViewMatrixInverseTranposed));
    //float3 viewLight3 = normalize(mul(float3(0, 1, 0), (float3x3) worldViewMatrixInverseTranposed));

    if (!outputDirectEnabled || outputDirectType == FLAG_OUTPUT_DIRECT_SOLID) {
        float lightNormalDotProduct = dot(viewNormal3, viewLight3); // for normalized input: range of -1 (down) to 1 (up)

        // Faces directed at right angle to sun receive this ratio of maximum direct light. Faces towards the sun get more, away from sun get less.
        float lightRatioAt90 = 0.25f; 
        float lightReceivedRatio;

        if (lightNormalDotProduct < 0) {
            lightReceivedRatio = (lightNormalDotProduct + 1) * lightRatioAt90;
        }
        else {
            lightReceivedRatio = (lightNormalDotProduct + lightRatioAt90) / (1 + lightRatioAt90);
        }

        float ambientLight2 = 0.14f; // TODO why is ambientLight even customizable outside of shader? needs to be balanced with lightRatioAt90 anyway
        lightReceivedRatio = (lightReceivedRatio + ambientLight2) / (1 + ambientLight2);

        //float lightReceivedRatio = max(0, lightNormalDotProduct + 0.3f) + ambientLight;
        float sunStrength = 1.7f;
        float staticStrength = 2.f;
        float staticLightAverage = (input.colLight.r + input.colLight.g + input.colLight.b) / 3.0f;
        output.light = ((lightReceivedRatio * sunStrength) * 0.5f) + ((staticLightAverage * staticStrength) * 0.5f);
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
        output.color = input.colLight;
    }
	return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

float CalcMipLevel(float2 texCoord)
{
    float2 dx = ddx(texCoord);
    float2 dy = ddy(texCoord);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));

    return max(0.0, 0.5 * log2(delta_max_sqr));
}

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
    // light color
    float4 lightColor = float4(float3(1, 1, 1) * input.light, 1);

    // albedo color
    float4 albedoColor;
    if (!outputDirectEnabled || outputDirectType == FLAG_OUTPUT_DIRECT_DIFFUSE) {
        albedoColor = baseColor.Sample(SampleType, input.uvBaseColor);

        // Alpha Cutoff
        // lower values will result in thinner coverage
        float alphaCutoff = 0.4;

        if (distantAlphaDensityFix) {
            // Distant Transparency Thickening
            // since non-blended transparency fades out on higher mips more than it should (potentially leading to disappearing vegetation), correct it
            float texWidth;
            float texHeight;
            baseColor.GetDimensions(texWidth, texHeight);
            float distAlphaMipScale = 0.25f;
            albedoColor.a *= 1 + CalcMipLevel(input.uvBaseColor * float2(texWidth, texHeight)) * distAlphaMipScale;
        }

        if (multisampleTransparency) {
            // Alpha to Coverage Sharpening
            // sharpen the multisampled alpha to remove banding and interior transparency; outputs close to 1 or 0
            albedoColor.a = (albedoColor.a - alphaCutoff) / max(fwidth(albedoColor.a), 0.0001) + 0.5;
        }

        // Alpha Test
        // if sharpening is used, the alphaCutoff its pretty much irrelevant here (we could just use 0.5)
        clip(albedoColor.a < alphaCutoff ? -1 : 1);
    }
    else if (outputDirectType == FLAG_OUTPUT_DIRECT_LIGHTMAP) {
        if (input.uvLightmap.z >= 0) /* dynamic branch */ {
            albedoColor = lightmaps.Sample(SampleType, input.uvLightmap);
        }
        else {
            albedoColor = input.color;
        }
    }
    else {
        albedoColor = input.color;
    }
    
    float4 shadedColor = albedoColor * lightColor;

    return shadedColor;
}