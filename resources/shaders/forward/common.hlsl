// Disable: pow(f, e) will not work for negative f, use abs(f) or conditionally handle negative values if you expect them
// We generally use pow(saturate(..), ..) which is always ok but still triggers warning.
#pragma warning (disable : 3571)

static const uint OUTPUT_FULL = 0;
static const uint OUTPUT_SOLID = 1;
static const uint OUTPUT_DIFFUSE = 2;
static const uint OUTPUT_NORMAL = 3;
static const uint OUTPUT_DEPTH = 4;
static const uint OUTPUT_LIGHT_SUN = 5;
static const uint OUTPUT_LIGHT_STATIC = 6;

cbuffer cbSettings : register(b0)
{
    float4 skyLight;
    float4 skyColor;
    
    float viewDistance;
    bool distanceFog;
    float distanceFogStart;
    float distanceFogEnd;
    float distanceFogSkyFactor;
    float distanceFogEaseOut;
    
    bool multisampleTransparency;
    bool distantAlphaDensityFix;

    bool reverseZ;
    uint outputType;
    bool outputAlpha;
    float alphaCutoff;

    float timeOfDay;
    bool skyTexBlur;
};

cbuffer cbCamera : register(b1)
{
    float4x4 worldViewMatrix;
    float4x4 worldViewMatrixInverseTranposed;
    float4x4 projectionMatrix;
};

static const uint BLEND_NONE = 0;
static const uint BLEND_ADD = 1;
static const uint BLEND_MULTIPLY = 2;
static const uint BLEND_ALPHA = 3;
static const uint BLEND_FACTOR = 4;

cbuffer cbBlendMode : register(b2)
{
    uint blendType;
}

cbuffer cbDebug : register(b3)
{
    float debugFloat1;
    float debugFloat2;
}

//--------------------------------------------------------------------------------------
// Util Functions
//--------------------------------------------------------------------------------------

// Inverse lerp
float inv_lerp(float from, float to, float value)
{
    return (clamp(value, from, to) - from) / (to - from);
}

//--------------------------------------------------------------------------------------
// Vertex Shader Functions
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Pixel Shader Functions
//--------------------------------------------------------------------------------------

float4 SwitchDiffuseByOutputMode(float4 texColor, float3 normalColor, float depthDistance)
{
    float3 diffuseColor;
    if (outputType == OUTPUT_FULL || outputType == OUTPUT_DIFFUSE) {
        diffuseColor = texColor.rgb;
    }
    else if (outputType == OUTPUT_NORMAL) {
        diffuseColor = normalColor;
    }
    else if (outputType == OUTPUT_DEPTH) {
        // using distance instead of actual Z-Buffer value (input.position.z) 
        diffuseColor = (float3) pow(saturate(1.f / pow(depthDistance, 1.2f)), 0.7f);
    }
    else {
        // full white material shading, TODO make adjustable
        float3 debugMaterialColor = (float3) 1;
        diffuseColor = debugMaterialColor;
    }
    
    float diffuseAlpha = outputAlpha ? texColor.a : 1;
    
    // prevent add/multiply pass from changing depth/normal information
    if ((blendType == BLEND_ADD || blendType == BLEND_MULTIPLY) && (outputType == OUTPUT_NORMAL || outputType == OUTPUT_DEPTH)) {
        diffuseColor = (float3) 0.5;// makes multiply invisible
        diffuseAlpha = 0;// makes add invisible
    }
    
    return float4(diffuseColor, diffuseAlpha);
}

float4 ApplyFog(float4 shadedColor, float depthDistance, bool isSky = false)
{
    static const float easeOutFogWorld = 0.85;
    static const float easeOutFogSky = 2.8;
    
    if (distanceFog && outputType == OUTPUT_FULL) {
        
        if (depthDistance > distanceFogStart) /* dynamic branch */ {
            float fogStart = min(distanceFogEnd - 0.00001, distanceFogStart);
            float weight = saturate((depthDistance - fogStart) / (distanceFogEnd - fogStart));
            // ease out
            float easeOutFactor = isSky ? easeOutFogSky + distanceFogEaseOut * 2 : easeOutFogWorld + distanceFogEaseOut;
            weight = 1 - pow(1.f - weight, easeOutFactor);
            
            if (blendType == BLEND_ADD) {
                // ADD blending must not add additional fog on top of already added fog in background,
                // and we want to fade out the added diffuse color so it does not "shine" through the fog.
                shadedColor.a *= (1.f - weight);
            }
            else if (blendType == BLEND_MULTIPLY) {
                // MULTIPLY blending is just lerped to invisible neutral value of 0.5
                shadedColor.rgb = lerp(shadedColor.rgb, (float3) 0.5f, weight);
            }
            else {
                // normal fog
                shadedColor.rgb = lerp(shadedColor.rgb, skyColor.rgb, weight);
            }
            
        }
    }
    return shadedColor;
}