#include "stdafx.h"
#include "MeshUtil.h"

#include <filesystem>

#include "../../Util.h"

namespace renderer::loader
{
    using std::array;
    using namespace DirectX;

    UV toUv(const ZenLib::ZMath::float2& source)
    {
        return UV{ source.x, source.y };
    }
    VEC3 toVec3(const ZenLib::ZMath::float3& source)
    {
        return VEC3{ source.x, source.y, source.z };
    }
    VEC3 toVec3(const ZenLib::ZMath::float3& source, float scale)
    {
        return VEC3{ source.x * scale, source.y * scale, source.z * scale };
    }
    VEC3 toVec3(const VEC4& vec4)
    {
        return VEC3{ vec4.x, vec4.y, vec4.z };
    }
    VEC3 toVec3(const XMVECTOR& xm4)
    {
        XMFLOAT4 result;
        XMStoreFloat4(&result, xm4);
        return VEC3{ result.x, result.y, result.z };
    }
    VEC4 toVec4(const XMVECTOR& xm4)
    {
        XMFLOAT4 result;
        XMStoreFloat4(&result, xm4);
        return VEC4{ result.x, result.y, result.z, result.w };
    }

    XMVECTOR calcFlatFaceNormal(const array<XMVECTOR, 3>& posXm) {
        // counter-clockwise winding, flip XMVector3Cross argument order for clockwise winding
        return  XMVector3Cross(XMVectorSubtract(posXm[2], posXm[0]), XMVectorSubtract(posXm[1], posXm[0]));
    }

    void warnIfNotNormalized(const XMVECTOR& source)
    {
        XMVECTOR normalized = XMVector3Normalize(source);
        XMVECTOR nearEqualMask = XMVectorNearEqual(source, normalized, XMVectorReplicate(.0001f));
        XMVECTOR nearEqualXm = XMVectorSelect(XMVectorReplicate(1.f), XMVectorReplicate(.0f), nearEqualMask);
        VEC4 nearEqual = toVec4(nearEqualXm);
        if (nearEqual.x != 0 || nearEqual.y != 0 || nearEqual.z != 0 || nearEqual.w != 0) {
            LOG(INFO) << "Vector was not normalized! " << source << "  |  " << normalized << "  |  " << nearEqualXm;
        }
    }

    float fromSRGB(const float channel) {
        return (channel <= 0.04045f) ? (channel / 12.92f) : pow((channel + 0.055f) / 1.055f, 2.4f);
    }

    D3DXCOLOR fromSRGB(const D3DXCOLOR color) {
        auto result = color;
        const float gamma = 2.2f;
        result.r = fromSRGB(result.r);
        result.g = fromSRGB(result.g);
        result.b = fromSRGB(result.b);
        return result;
    }

    D3DXCOLOR greyscale(const float channel) {
        return D3DXCOLOR(channel, channel, channel, 1);
    }
}