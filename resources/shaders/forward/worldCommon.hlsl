#include "common.hlsl"

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

void ClipIfNotInLodRange(uint2 pos, float distance)
{
    if (rangeType != LOD_RANGE_NONE)
    {
        bool doClip = false;
        static const float LOD_RADIUS_OVERLAP = 0.9f; // fix gaps, for example: G2 Khorinis fishing boats  
        float distWithOverlap = distance + (LOD_RADIUS_OVERLAP * 0.5f * (rangeType == LOD_RANGE_MAX ? -1.f : 1.f));
        
        if (ditherEnabled)
        {
            float ditherWeight = inv_lerp(rangeBegin, rangeEnd, distWithOverlap);
            doClip = (rangeType == LOD_RANGE_MAX) == DitherTest(pos, ditherWeight);
        }
        else
        {
            // rangeBegin == rangeEnd
            doClip = (rangeType == LOD_RANGE_MAX) == (rangeBegin <= distWithOverlap);
        }
        
        clip(doClip ? -1 : 1);
    }
}