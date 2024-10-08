#pragma once

#include <DirectXMath.h>

#include "Common.h"
#include "../Util.h"

#include "utils/mathlib.h"

namespace render
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
    DirectX::XMMATRIX toXMM(const ZenLib::ZMath::Matrix& matrix);

    UV from(const ZenLib::ZMath::float2& source);
    VEC3 from(const ZenLib::ZMath::float3& source);
    VEC3 from(const ZenLib::ZMath::float3& source, float scale);

    VEC3 toVec3(const DirectX::XMFLOAT3& xmf3);
    VEC3 toVec3(const DirectX::XMVECTOR& xm4);
    VEC4 toVec4(const DirectX::XMVECTOR& xm4);

    DirectX::XMVECTOR centroidPos(const std::array<DirectX::XMVECTOR, 3>& posXm);
    DirectX::XMVECTOR calcFlatFaceNormal(const std::array<DirectX::XMVECTOR, 3>& posXm);
    ChunkIndex toChunkIndex(DirectX::XMVECTOR posXm);

    inline std::ostream& operator <<(std::ostream& os, const DirectX::XMVECTOR& that)
    {
        DirectX::XMFLOAT4 float4;
        DirectX::XMStoreFloat4(&float4, that);
        return os << "[X:" << float4.x << " Y:" << float4.y << " Z:" << float4.z << " W:" << float4.w << "]";
    }

    template <typename C1, typename C2>
    void insert(VERTEX_DATA_BY_MAT& target, const Material& material, const C1& positions, const C2& normalsAndUvs)
    {
        auto& meshData = ::util::getOrCreateDefault(target, material);

        meshData.vecPos.insert(meshData.vecPos.end(), positions.begin(), positions.end());
        meshData.vecOther.insert(meshData.vecOther.end(), normalsAndUvs.begin(), normalsAndUvs.end());
    }

    template <typename C1, typename C2>
    void insert(VERT_CHUNKS_BY_MAT& target, const Material& material, const C1& positions, const C2& normalsAndUvs)
    {
        std::unordered_map<ChunkIndex, VEC_VERTEX_DATA>& meshData = ::util::getOrCreateDefault(target, material);
        
        for (uint32_t i = 0; i < positions.size(); i += 3) {
            const ChunkIndex chunkIndex = toChunkIndex(centroidPos({
                toXM4Pos(positions[i]),
                toXM4Pos(positions[i + 1]),
                toXM4Pos(positions[i + 2])
            }));
            VEC_VERTEX_DATA& chunkData = ::util::getOrCreateDefault(meshData, chunkIndex);

            // TODO copied from MeshFromVdfLoader for now, clean up
            chunkData.vecPos.push_back(positions[i]);
            chunkData.vecPos.push_back(positions[i + 1]);
            chunkData.vecPos.push_back(positions[i + 2]);
            chunkData.vecOther.push_back(normalsAndUvs[i]);
            chunkData.vecOther.push_back(normalsAndUvs[i + 1]);
            chunkData.vecOther.push_back(normalsAndUvs[i + 2]);
        }
    }

    void warnIfNotNormalized(const DirectX::XMVECTOR& source);
}