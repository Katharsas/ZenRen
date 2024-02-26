#include "stdafx.h"
#include "MeshUtil.h"

#include <filesystem>

#include "../../Util.h"

namespace renderer::loader
{
    using namespace DirectX;

    UV from(const ZenLib::ZMath::float2& source)
    {
        return UV{ source.x, source.y };
    }
    VEC3 from(const ZenLib::ZMath::float3& source)
    {
        return VEC3{ source.x, source.y, source.z };
    }
    VEC3 toVec3(const DirectX::XMVECTOR& xm4)
    {
        DirectX::XMFLOAT4 result;
        DirectX::XMStoreFloat4(&result, xm4);
        return VEC3{ result.x, result.y, result.z };
    }
    VEC4 toVec4(const DirectX::XMVECTOR& xm4)
    {
        DirectX::XMFLOAT4 result;
        DirectX::XMStoreFloat4(&result, xm4);
        return VEC4{ result.x, result.y, result.z, result.w };
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
}