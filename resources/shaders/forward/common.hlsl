static const uint OUTPUT_FULL = 0;
static const uint OUTPUT_SOLID = 1;
static const uint OUTPUT_DIFFUSE = 2;
static const uint OUTPUT_NORMAL = 3;
static const uint OUTPUT_LIGHT_SUN = 4;
static const uint OUTPUT_LIGHT_STATIC = 5;

cbuffer cbSettings : register(b0)
{
    float4 skyLight;
    
    bool multisampleTransparency;
    bool distantAlphaDensityFix;

    uint outputType;

    float timeOfDay;
    bool skyTexBlur;
};

cbuffer cbCamera : register(b1)
{
    float4x4 worldViewMatrix;
    float4x4 worldViewMatrixInverseTranposed;
    float4x4 projectionMatrix;
};

cbuffer cbBlendMode : register(b2)
{
    bool alphaTest; // false => blending enabled, so pixel shader alpha should not be modifed
}

cbuffer cbDebug : register(b3)
{
    float debugFloat1;
    float debugFloat2;
}