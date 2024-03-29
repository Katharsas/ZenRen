#pragma once

#include <DirectXMath.h>

#include "../RendererCommon.h"

#include "utils/mathlib.h"

namespace renderer::loader
{
    template <typename T> bool isZero(const T& vec3, float threshold)
    {
        return std::abs(vec3.x) <= threshold && std::abs(vec3.y) <= threshold && std::abs(vec3.z) <= threshold;
    }

    template <typename T> DirectX::XMVECTOR toXM4Pos(const T& vec3)
    {
        return DirectX::XMVectorSet(vec3.x, vec3.y, vec3.z, 1);
    }
    template <typename T> DirectX::XMVECTOR toXM4Dir(const T& vec3)
    {
        return DirectX::XMVectorSet(vec3.x, vec3.y, vec3.z, 0);
    }

    UV from(const ZenLib::ZMath::float2& source);
    VEC3 from(const ZenLib::ZMath::float3& source);

    VEC3 toVec3(const DirectX::XMVECTOR& xm4);
    VEC4 toVec4(const DirectX::XMVECTOR& xm4);

    inline std::ostream& operator <<(std::ostream& os, const DirectX::XMVECTOR& that)
    {
        DirectX::XMFLOAT4 float4;
        DirectX::XMStoreFloat4(&float4, that);
        return os << "[X:" << float4.x << " Y:" << float4.y << " Z:" << float4.z << " W:" << float4.w << "]";
    }

    template <typename C1, typename C2>
    void insert(std::unordered_map<Material, VEC_VERTEX_DATA>& target, const Material& material, const C1& positions, const C2& normalsAndUvs)
    {
        auto& meshData = ::util::getOrCreate(target, material);
        meshData.vecPos.insert(meshData.vecPos.end(), positions.begin(), positions.end());
        meshData.vecNormalUv.insert(meshData.vecNormalUv.end(), normalsAndUvs.begin(), normalsAndUvs.end());
    }

    void warnIfNotNormalized(const DirectX::XMVECTOR& source);
}