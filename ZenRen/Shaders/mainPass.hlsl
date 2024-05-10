//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

static const uint FLAG_OUTPUT_SOLID = 0;
static const uint FLAG_OUTPUT_DIFFUSE = 1;
static const uint FLAG_OUTPUT_NORMAL = 2;
static const uint FLAG_OUTPUT_LIGHT_SUN = 3;
static const uint FLAG_OUTPUT_LIGHT_DYNAMIC = 4;
static const uint FLAG_OUTPUT_LIGHT_STATIC = 5;

struct Light {
    float3 pos;
    float range;
    float4 color;
};

cbuffer cbSettings : register(b0) {
    bool multisampleTransparency;
    bool distantAlphaDensityFix;

    bool debugOutput;
    uint debugOutputType;
};

cbuffer cbPerObject : register(b1)
{
    float4x4 worldViewMatrix;
    float4x4 worldViewMatrixInverseTranposed;
    float4x4 projectionMatrix;
};

StructuredBuffer<Light> lightsDynamic : register(t2);

struct VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL0;
    float2 uvBaseColor : TEXCOORD0;
    float3 uvLightmap : TEXCOORD1;
    float4 colLight : COLOR;
    float3 dirLight : NORMAL1;
    float sunLight : TEXCOORD2;
};

struct VS_OUT
{
    float2 uvBaseColor : TEXCOORD0;
    float3 uvLightmap : TEXCOORD1;
    float4 color : COLOR;
    float3 light : LIGHT_INTENSITY;
    //float4 normal : NORMAL;
    float4 position : SV_POSITION;
};

// Returns factor for amount of light received (between 0 and 1).
// To simulate bounce lighting, backside faces also get a bit of light and there is a fixed ambient amount.
float CalcLightDirectional(float3 dirLight, float3 dirNormal, float lightRatioAt90, float ambientLight)
{
    // for normalized input: range of -1 (down) to 1 (up)
    float lightNormalDotProduct = dot(dirNormal, dirLight);

    // Normals directed at right angle receive this ratio of maximum direct light. Normals closer to dirLight get more, away get less.
    //float lightRatioAt90 = 0.25f;

    float lightReceivedRatio;
    if (lightNormalDotProduct < 0) {
        lightReceivedRatio = (lightNormalDotProduct + 1) * lightRatioAt90;
    }
    else {
        lightReceivedRatio = (lightNormalDotProduct + lightRatioAt90) / (1 + lightRatioAt90);
    }
    lightReceivedRatio = (lightReceivedRatio + ambientLight) / (1 + ambientLight);
    return lightReceivedRatio;
}

float CalcLightSun(float3 dirLight, float3 dirNormal)
{
    float lightReceived = CalcLightDirectional(dirLight, dirNormal, 0.6f, 0.3f);
    //float strength = 2.f;
    float strength = 1.0f;
    return lightReceived * strength;
}

float CalcLightDynamic(Light light, float3 pos, float3 dirNormal)
{
    float rangeReciprocal = 1.f / light.range;

    float3 posToLight = light.pos - pos;
    float distance = length(posToLight);
    float3 dirLight = posToLight / distance;// Normalize
    
    // Attenuation
    float nearness = 1.f - saturate(distance * rangeReciprocal);
    float distanceFactor = nearness * nearness;

    // Normal
    float normalFactor = CalcLightDirectional(dirLight, dirNormal, 0.4f, 0.f);

    float lightReceived = distanceFactor * normalFactor;
    float strength = 3.0f;
    return lightReceived * strength;
}

float CalcLightStaticVob(float3 dirLight, float3 dirNormal)
{
    float lightReceived = CalcLightDirectional(dirLight, dirNormal, 0.f, 0.f);
    //float strength = 2.1f;
    float strength = 5.5f;
    return lightReceived * strength;
}

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

    bool enableLightSun = false;
    bool enableLightDynamic = false;
    bool enableLightStatic = false;
    bool isVob = (input.dirLight.x > -99);

    if (!debugOutput || debugOutputType == FLAG_OUTPUT_SOLID) {
        enableLightSun = true;
        enableLightDynamic = true;
        enableLightStatic = true;
    }
    if (debugOutput && debugOutputType == FLAG_OUTPUT_LIGHT_SUN) {
        enableLightSun = true;
    }
    if (debugOutput && debugOutputType == FLAG_OUTPUT_LIGHT_DYNAMIC) {
        enableLightDynamic = true;
    }
    if (debugOutput && debugOutputType == FLAG_OUTPUT_LIGHT_STATIC) {
        enableLightStatic = true;
    }

    float3 lightSun = (float3) (input.sunLight * CalcLightSun(viewLight3, viewNormal3));
    float3 lightDynamic = (float3) 0.f;
    float3 lightStatic;
    float3 lightAmbient;
    if (isVob) {
        //lightStatic = input.colLight * CalcLightStaticVob(input.dirLight, viewNormal3);
        //lightAmbient = input.colLight * 0.16f;// original SRGB factor = 0.4f
        lightStatic - 0.f;
        lightAmbient = 0.f;
    }
    else {
        lightStatic = input.colLight;
        //lightStatic = 0.f;
        lightAmbient = 0.f;
    }
    for (uint i = 0; i < 123; i++) {
        Light light = lightsDynamic[i];
        lightDynamic += ((float3) light.color) * CalcLightDynamic(light, (float3) input.position, viewNormal3);
    }

    output.light = (float3) 0;
    if (enableLightSun) {
        output.light += lightSun * 0.25f;
    }
    if (enableLightDynamic) {
        output.light += lightDynamic * 0.7f;
    }
    if (enableLightStatic) {
        output.light += (lightAmbient + lightStatic) * 0.7f;
    }
    if (!(enableLightSun || enableLightDynamic || enableLightStatic)) {
        output.light = (float3) 1;
    }
    output.light *= 1.1;// TODO do this in tonemapping stage or something

    output.position = mul(viewPosition, projectionMatrix);
    output.uvBaseColor = input.uvBaseColor;
    output.uvLightmap = input.uvLightmap;
   
    if (debugOutputType == FLAG_OUTPUT_NORMAL) {
        float3 normalColor = ((float3) input.normal + 1) / 2;
        output.color = float4(normalColor * 0.75f, 1);
    }
    else {
        output.color = (float4) 1;
    }
	return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

Texture2D baseColor : register(t0);
Texture2DArray lightmaps : register(t1);
SamplerState SampleType : register(s0);

struct PS_IN
{
    float2 uvBaseColor : TEXCOORD0;
    float3 uvLightmap : TEXCOORD1;
    float4 color : COLOR;
    float3 light : LIGHT_INTENSITY;
    //int indexLightmap : INDEX_LIGHTMAP;
    //float4 normal : NORMAL;
};

float CalcMipLevel(float2 texCoord)
{
    float2 dx = ddx(texCoord);
    float2 dy = ddy(texCoord);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));

    return max(0.0f, 0.5f * log2(delta_max_sqr));
}

float CalcDistantAlphaDensityFactor(Texture2D<float4> sourceTex, float2 texCoord)
{
    // Distant Transparency Thickening
    // since non-blended transparency fades out on higher mips more than it should (potentially leading to disappearing vegetation), correct it
    if (distantAlphaDensityFix) {
        float texWidth;
        float texHeight;
        sourceTex.GetDimensions(texWidth, texHeight);// TODO this info could be passed with cb
        float distAlphaMipScale = 0.25f;
        return 1 + CalcMipLevel(texCoord * float2(texWidth, texHeight)) * distAlphaMipScale;
    }
    else {
        return 1;
    }
}

float AlphaTestAndSharpening(float alpha)
{
    // Alpha Cutoff - lower values will result in thinner coverage
    float alphaCutoff = 0.4;

    // Alpha to Coverage Sharpening
    // sharpen the multisampled alpha to remove banding and interior transparency; outputs close to 1 or 0
    if (multisampleTransparency) {
        alpha = (alpha - alphaCutoff) / max(fwidth(alpha), 0.0001) + 0.5;
    }

    // Alpha Test - if sharpening is used, the alphaCutoff its pretty much irrelevant here (we could just use 0.5)
    clip(alpha < alphaCutoff ? -1 : 1);
    return alpha;
}

float4 SampleLightmap(float3 uvLightmap)
{
    // TODO
    // - We should probably use a LOD/mipmap bias, not a fixed mimap level, does SampleLevel do that?
    // - How does Gothic get the smoothed look? any mipmap multisampling trickery?

    //return lightmaps.Sample(SampleType, uvLightmap);
    return lightmaps.SampleLevel(SampleType, uvLightmap, 0.5f);
}

float4 PS_Main(PS_IN input) : SV_TARGET
{
    // light color
    float4 lightColor = float4(input.light, 1);

    if (!debugOutput
            || debugOutputType == FLAG_OUTPUT_SOLID
            || debugOutputType == FLAG_OUTPUT_LIGHT_STATIC) {
        if (input.uvLightmap.z >= 0) /* dynamic branch */ {
            lightColor = SampleLightmap(input.uvLightmap);
        }
    }

    // diffuse (calculate always so alpha clipping works regardless of debugOutputType)
    float4 diffuseColor;
    {
        diffuseColor = baseColor.Sample(SampleType, input.uvBaseColor);
        diffuseColor.a *= CalcDistantAlphaDensityFactor(baseColor, input.uvBaseColor);
        diffuseColor.a = AlphaTestAndSharpening(diffuseColor.a);
    }

    // albedo color
    float4 albedoColor;
    if (!debugOutput || debugOutputType == FLAG_OUTPUT_DIFFUSE) {
        albedoColor = diffuseColor;
    }
    else {
        albedoColor = input.color;
    }
    
    float4 shadedColor = albedoColor * lightColor;
    return shadedColor;
}