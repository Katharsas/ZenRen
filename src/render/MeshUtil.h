#pragma once

#include <DirectXMath.h>

#include "Common.h"
#include "../Util.h"

namespace render
{
    template <Vec3 Vec>
    bool isZero(const Vec& vec, float threshold)
    {
        return std::abs(vec.x) <= threshold
            && std::abs(vec.y) <= threshold
            && std::abs(vec.z) <= threshold;
    }

    template <Vec3 Vec>
    DirectX::XMVECTOR toXM4Pos(const Vec& vec)
    {
        return DirectX::XMVectorSet(vec.x, vec.y, vec.z, 1);
    }
    template <Vec3 Vec>
    DirectX::XMVECTOR toXM4Dir(const Vec& vec)
    {
        return DirectX::XMVectorSet(vec.x, vec.y, vec.z, 0);
    }

    DirectX::XMMATRIX toXMMatrix(const float * matrix);

    template <Vec2 Vec>
    UV toUv(const Vec& vec)
    {
        return { vec.x, vec.y };
    }
    template <Vec2 Vec>
    VEC2 toVec2(const Vec& vec)
    {
        return { vec.x, vec.y };
    }
    template <Vec3 Vec> 
    VEC3 toVec3(const Vec& vec)
    {
        return { vec.x, vec.y, vec.z };
    }
    template <Vec3 Vec>
    VEC3 toVec3(const Vec& vec, float scale)
    {
        return { vec.x * scale, vec.y * scale, vec.z * scale };
    }

    VEC3 toVec3(const DirectX::XMFLOAT3& xmf3);
    VEC3 toVec3(const DirectX::XMVECTOR& xm4);
    VEC4 toVec4(const DirectX::XMVECTOR& xm4);

    bool isZero(const DirectX::XMVECTOR& vec, float threshold);
    DirectX::XMVECTOR bboxCenter(const std::array<DirectX::XMVECTOR, 2>& bbox);
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

    void printXMMatrix(const DirectX::XMMATRIX& matrix);
}