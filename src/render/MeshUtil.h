#pragma once

#include <DirectXMath.h>

#include "Common.h"
#include "../Util.h"

namespace render
{
    // TODO split into math util and mesh util or move some stuff to Primitive.h?

    template <XYZ V3>
    bool isZero(const V3& vec, float threshold)
    {
        return std::abs(vec.x) <= threshold
            && std::abs(vec.y) <= threshold
            && std::abs(vec.z) <= threshold;
    }

    template <XYZ V3>
    DirectX::XMVECTOR toXM4Pos(const V3& vec)
    {
        return DirectX::XMVectorSet(vec.x, vec.y, vec.z, 1);
    }
    template <XYZ V3>
    DirectX::XMVECTOR toXM4Dir(const V3& vec)
    {
        return DirectX::XMVectorSet(vec.x, vec.y, vec.z, 0);
    }

    DirectX::XMMATRIX toXMMatrix(const float * matrix);

    DirectX::XMFLOAT3 toFloat3(const DirectX::XMVECTOR& xm4);

    template <XY V2>
    Uv toUv(const V2& vec)
    {
        return { vec.x, vec.y };
    }
    template <XY V2>
    Vec2 toVec2(const V2& vec)
    {
        return { vec.x, vec.y };
    }
    template <XYZ V3>
    Vec3 toVec3(const V3& vec)
    {
        return { vec.x, vec.y, vec.z };
    }
    template <XYZ V3>
    Vec3 toVec3(const V3& vec, float scale)
    {
        return { vec.x * scale, vec.y * scale, vec.z * scale };
    }

    Vec3 toVec3(const DirectX::XMFLOAT3& xmf3);
    Vec3 toVec3(const DirectX::XMVECTOR& xm4);
    Vec4 toVec4(const DirectX::XMVECTOR& xm4);

    bool isZero(const DirectX::XMVECTOR& vec, float threshold);
    DirectX::XMVECTOR bboxCenter(const std::array<DirectX::XMVECTOR, 2>& bbox);
    DirectX::XMVECTOR centroidPos(const std::array<DirectX::XMVECTOR, 3>& posXm);
    DirectX::XMVECTOR calcFlatFaceNormal(const std::array<DirectX::XMVECTOR, 3>& posXm);
    GridPos toGridPos(const grid::Grid& grid, DirectX::XMVECTOR posXm);

    inline std::ostream& operator <<(std::ostream& os, const DirectX::XMVECTOR& that)
    {
        DirectX::XMFLOAT4 float4;
        DirectX::XMStoreFloat4(&float4, that);
        return os << "[X:" << float4.x << " Y:" << float4.y << " Z:" << float4.z << " W:" << float4.w << "]";
    }

    template <typename T>
    DirectX::BoundingBox createBboxFromPoints(const std::vector<T>& positions)
    {
        DirectX::BoundingBox result;
        DirectX::BoundingBox::CreateFromPoints(result, positions.size(), (const DirectX::XMFLOAT3*)positions.data(), sizeof(T));
        return result;
    }

    template <typename C1, typename C2>
    void insert(MatToVertsBasic& target, const Material& material, const C1& positions, const C2& normalsAndUvs)
    {
        auto& meshData = ::util::getOrCreateDefault(target, material);

        meshData.vecPos.insert(meshData.vecPos.end(), positions.begin(), positions.end());
        meshData.vecOther.insert(meshData.vecOther.end(), normalsAndUvs.begin(), normalsAndUvs.end());
    }

    template <typename C1, typename C2>
    void insert(MatToChunksToVertsBasic& target, const Material& material, const C1& positions, const C2& normalsAndUvs)
    {
        std::unordered_map<GridPos, VertsBasic>& meshData = ::util::getOrCreateDefault(target, material);
        
        for (uint32_t i = 0; i < positions.size(); i += 3) {
            const GridPos gridPos = { 0, 0 };// TODO this is kinda bad
            VertsBasic& chunkData = ::util::getOrCreateDefault(meshData, gridPos);

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

    void printXMMatrix(const DirectX::XMMATRIX& matrix, const std::string& numberFormat);
}