#include "common.hlsl"

//cbuffer cbWorldSettings : register(b5)
//{
//    bool enablePerPixelLod;
//    bool enableLodDithering;
//}

static const uint LOD_RANGE_NONE = 0;
static const uint LOD_RANGE_MAX = 1;
static const uint LOD_RANGE_MIN = 2;

cbuffer cbLodRange : register(b5)
{
    uint1 rangeType;
    bool ditherEnabled;
    float rangeBegin;
    float rangeEnd;
}

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------

struct VS_IN
{
    float4 position : POSITION;
    float4 normal : NORMAL0;
    float2 uvTexColor : TEXCOORD0;
    float3 uviTexLightmap : TEXCOORD1;
    float4 colLight : COLOR;
    float3 dirLight : NORMAL1;
    float sunLight : TEXCOORD2;
    uint1 iTexColor : TEXCOORD3;
};

struct VS_OUT
{
    nointerpolation float iTexColor : TEX_INDEX0;
    nointerpolation float iTexLightmap : TEX_INDEX1;
    
    float2 uvTexColor : TEXCOORD0;
    float2 uvTexLightmap : TEXCOORD1;
    float3 light : LIGHT_INTENSITY;
    float distance : DISTANCE;
    float4 position : SV_POSITION;
};

// Returns factor for amount of light received (between 0 and 1).
// To simulate bounce lighting, backside faces also get a bit of light and there is a fixed ambient amount.
float CalcLightDirectional(float3 dirLight, float3 dirNormal, float lightRatioAt90)
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
    return lightReceivedRatio;
}

// Keeps max, anything below or above max is pushed towards max by ambientLight, light is scaled by (max - ambientLight).
// This means a light value of 1 with a max of 1.5 will result in 1.5 regardless of ambientLight.
float3 RescaleOntoAmbient(float3 light, float3 ambientLight, float max = 1)
{
    return ambientLight + (((float3) max - ambientLight) * light);

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

    bool isVob = input.dirLight.x > -99;// world sets dirLight to -100
    bool hasLightmap = input.uviTexLightmap.z >= 0;
    bool hasVertexLight = !isVob && !hasLightmap;

    if (outputType == OUTPUT_FULL || outputType == OUTPUT_SOLID || outputType == OUTPUT_LIGHT_SUN) {
        float3 light = (float3) 1.f;
        float3 lightColor = input.colLight.rgb;
        if (!isVob) {
            light = RescaleOntoAmbient(lightColor * .9f, (float3) .06f);
        }
        else {
            float lightReceived = CalcLightDirectional(input.dirLight, viewNormal3, .0f);
            float3 directionalLight = lightColor * lightReceived;
            if (blendType != BLEND_ADD) {
                light = RescaleOntoAmbient(directionalLight, lightColor * .17f, 1.25f);
            }
            else {
                // TODO this is just hacking around the fact that ADD decals just look way less bright in original (always or not?)
                light = directionalLight * 0.4f;
            }
        }
        // TODO it is unclear if vob ambient factor is applied before or after skylight
        output.light = light * skyLight.rgb;
    }
    else if (outputType == OUTPUT_NORMAL) {
        output.light = input.normal.xyz;
    }
    else {
        output.light = (float3) 1;
    }

    output.position = mul(viewPosition, projectionMatrix);
    
    // Use View length since it is independent of near/far plane and relative to camera origin (circular, stable during rotations).
    // Projection length would be relative to planes, Z-value alone would be non-circular (unstable during rotations for big FOV angles).
    // Projection-Z can be easily obtained in Pixel Shader by reading SV_POSITION Z (use only Z, not W).
    // See: https://learn.microsoft.com/en-us/windows/win32/direct3d9/projection-transform
    // See: https://developer.download.nvidia.com/assets/gamedev/docs/Fog2.pdf
    output.distance = length(viewPosition);
    
    output.uvTexColor = input.uvTexColor;
    output.iTexColor = input.iTexColor;
    output.uvTexLightmap = input.uviTexLightmap.xy;
    output.iTexLightmap = input.uviTexLightmap.z;
   
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
    // TODO values that do not need to be interpolated like tex array index should be marked as such
    nointerpolation float iTexColor : TEX_INDEX0;
    nointerpolation float iTexLightmap : TEX_INDEX1;
    
    float2 uvTexColor : TEXCOORD0;
    float2 uvTexLightmap : TEXCOORD1; // TODO should be called uviTexLightmap
    float3 light : LIGHT_INTENSITY;
    
    float distance : DISTANCE;
    float4 position : SV_POSITION;
};

// TODO try blue noise 16x16, see:
//   https://github.com/libjxl/libjxl/issues/3926
//   https://momentsingraphics.de/BlueNoise.html#Downloads
//   https://www.shadertoy.com/view/M3BBWh
// TODO try temporal offset/randomness (so pattern is not fixed to camera/screen)
float Dither4x4Ordered(uint2 pos)
{
    static const float bayerConstants[4][4] =
    {
        { 0.0625, 0.5625, 0.1875, 0.6875 },
        { 0.8125, 0.3125, 0.9375, 0.4375 },
        { 0.2500, 0.7500, 0.1250, 0.6250 },
        { 1.0000, 0.5000, 0.8750, 0.3750 },
    };
    pos = pos % 4;
    return bayerConstants[pos.x][pos.y];
}

bool DitherTest(uint2 pos, float weight)
{
    float limit = Dither4x4Ordered(pos);
    // add half step so result is evenly mapped onto range [0-1], otherwise even tiny inaccuracy can impact result for weight == 1
    return saturate(weight + 0.03125) - limit >= 0;
}

float CalcMipLevel(float2 texCoord)
{
    float2 dx = ddx(texCoord);
    float2 dy = ddy(texCoord);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));

    return max(0.0f, 0.5f * log2(delta_max_sqr));
}

float CalcDistantAlphaDensityFactor(Texture2DArray<float4> sourceTex, float2 texCoord)
{
    // Distant Transparency Thickening
    // since non-blended transparency fades out on higher mips more than it should (potentially leading to disappearing vegetation), correct it
    if (distantAlphaDensityFix) {
        float texWidth;
        float texHeight;
        float elements;
        sourceTex.GetDimensions(texWidth, texHeight, elements); // TODO this info could be passed with cb/sb
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
    // TODO make configurable
    float alphaCutoff = 0.4;

    // Alpha to Coverage Sharpening
    // sharpen the multisampled alpha to remove banding and interior transparency; outputs close to 1 or 0
    if (multisampleTransparency) {
        // TODO sharpening should be experimented with, especially since it massively changes perceived densitity
        //      and since it seems to only have an actual positive effect if texture resolution is high enough for sampler
        alpha = (alpha - alphaCutoff) / max(fwidth(alpha), 0.0001) + 0.5;
        // Clipping will be effectively performed by  Alpha-To-Coverage based on output alpha
    }
    else {
        clip(alpha < alphaCutoff ? -1 : 1);
    }
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

void ClipIfNotInLodRange(uint2 pos, float distance)
{
    if (rangeType != LOD_RANGE_NONE) {
        bool doClip = false;
        static const float LOD_RADIUS_OVERLAP = 0.9f; // fix gaps, for example: G2 Khorinis fishing boats  
        float distWithOverlap = distance + (LOD_RADIUS_OVERLAP * 0.5f * (rangeType == LOD_RANGE_MAX ? -1.f : 1.f));
        
        if (ditherEnabled) {
            float ditherWeight = inv_lerp(rangeBegin, rangeEnd, distWithOverlap);
            doClip = (rangeType == LOD_RANGE_MAX) == DitherTest(pos, ditherWeight);
        }
        else {
            // rangeBegin == rangeEnd
            doClip = (rangeType == LOD_RANGE_MAX) == (rangeBegin <= distWithOverlap);
        }
        
        clip(doClip ? -1 : 1);
    }
}

float4 PS_Main(PS_IN input) : SV_TARGET
{
    // do our own circular clipping (stable during rotations)
    clip(viewDistance - input.distance);
    
    // LOD
    ClipIfNotInLodRange(input.position.xy, input.distance);
    
    // light color
    float4 lightColor;
    if (outputType == OUTPUT_NORMAL) {
        lightColor = float4((float3) 0.5, 1);
    }
    else {
        lightColor = float4(input.light, 1);

        if (outputType == OUTPUT_FULL
            || outputType == OUTPUT_SOLID
            || outputType == OUTPUT_LIGHT_STATIC) {
            
            if (input.iTexLightmap >= 0) /* dynamic branch */ {
                lightColor = SampleLightmap(float3(input.uvTexLightmap, input.iTexLightmap));
            }
        }
    }

    // diffuse (calculate always so alpha clipping works regardless of outputType)
    float4 diffuseColor;
    {
        diffuseColor = baseColor.Sample(SampleType, float3(input.uvTexColor, input.iTexColor));
        if (blendType == BLEND_NONE) {
            // TODO if this costs significant performance we should separate alpha tested geometry from fully opaque geometry
            diffuseColor.a *= CalcDistantAlphaDensityFactor(baseColor, input.uvTexColor);
            diffuseColor.a = AlphaTestAndSharpening(diffuseColor.a);
        }
    }

    diffuseColor = SwitchDiffuseByOutputMode(diffuseColor, input.light, input.distance);
    
    float4 shadedColor = diffuseColor * lightColor;
    
    shadedColor = ApplyFog(shadedColor, input.distance);
    
    // TODO additive blending could use its own shader anyway since it basically does not need a ton of vertex features
    // and basically has its own alpha and light adjustment code?
    
    return shadedColor; //float4(shadedColor.rgb, 1);
}