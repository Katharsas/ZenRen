// see VertexPacker.cpp for packed format description

struct VS_IN
{
    float4 position : POSITION;
    
#if VERTEX_INPUT_RAW 
    float4 normal : NORMAL0;
#else
    uint normal : NORMAL0;
#endif   
    
#if VERTEX_INPUT_RAW
    float2 uvTexColor : TEXCOORD0;
#else
    uint uvTexColor : TEXCOORD0;
#endif
    
    float3 uviTexLightmap : TEXCOORD1;
    float4 colLight : COLOR;
    float3 dirLight : NORMAL1;
    float sunLight : TEXCOORD2;
    uint iTexColor : TEXCOORD3;
};

float3 unpackNormal(VS_IN input)
{
#if VERTEX_INPUT_RAW
    return input.normal.xyz;
#else
    static const uint normalComponentBits = 14;
    static const uint normalComponentMask = (1 << normalComponentBits) - 1;
    
    uint packed = input.normal;
    uint xQuant = packed & normalComponentMask;
    uint xSign = (packed >> normalComponentBits) & 1;
    uint zQuant = (packed >> normalComponentBits + 1) & normalComponentMask;
    uint zSign = (packed >> normalComponentBits * 2 + 1) & 1;
    uint ySign = (packed >> normalComponentBits * 2 + 2) & 1;

    float invMaxVal = 1.f / normalComponentMask;
    float xAbs = xQuant * invMaxVal;
    float zAbs = zQuant * invMaxVal;

    float yAbsSquared = 1.0f - xAbs * xAbs - zAbs * zAbs;
    float yAbs = (yAbsSquared > 0.f) ? sqrt(yAbsSquared) : 0.f;

    return float3(
        xSign ? -xAbs : xAbs,
        ySign ? -yAbs : yAbs,
        zSign ? -zAbs : zAbs
	);
#endif
}

float2 unpackUvTexColor(VS_IN input)
{
#if VERTEX_INPUT_RAW
    return input.uvTexColor;
#else
    static const uint uvExponentBits = 4;
    static const uint uvComponentBits = 14;
    static const uint uvExponentMask = (1 << uvExponentBits) - 1;
    static const uint uvComponentMask = (1 << uvComponentBits) - 1;
    static const uint scale = (1 << (uvComponentBits - 1));
    
    uint packed = input.uvTexColor;
    uint exponent = ((packed >> (uvComponentBits * 2)) & uvExponentMask);
    uint uBits = (packed >> uvComponentBits) & uvComponentMask;
    uint vBits = packed & uvComponentMask;
    static const uint signMask = 1 << (uvComponentBits - 1);
    static const uint signExtend = ~uvComponentMask;
	// extend sign (1 in highest bit) to all higher bits (2's complement)
    float2 uvNorm =
    {
        (int) (uBits & signMask ? uBits | signExtend : uBits),
	    (int) (vBits & signMask ? vBits | signExtend : vBits),
    };
    float factor = 1 << exponent;
    return uvNorm * (factor / scale);
#endif
}