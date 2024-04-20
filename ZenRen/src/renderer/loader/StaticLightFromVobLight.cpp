#include "stdafx.h"
#include "StaticLightFromVobLight.h"

#include "MeshUtil.h"

namespace renderer::loader
{
    using namespace DirectX;
    using ::std::vector;
    using ::std::unordered_map;

    bool rayIntersectsWorldFaces(XMVECTOR rayStart, XMVECTOR rayEnd, float maxDistance, const unordered_map<Material, VEC_VERTEX_DATA>& meshData, const VertLookupTree& vertLookup)
    {
        XMVECTOR direction = DirectX::XMVector3Normalize(rayEnd - rayStart);
        vector<VertKey> vertKeys = rayIntersected(vertLookup, rayStart, rayEnd);
        for (auto& vertKey : vertKeys) {
            auto& face = vertKey.getPos(meshData);
            XMVECTOR faceA = toXM4Pos(face[0]);
            XMVECTOR faceB = toXM4Pos(face[1]);
            XMVECTOR faceC = toXM4Pos(face[2]);
            float distance;
            bool intersects = TriangleTests::Intersects(rayStart, direction, faceA, faceB, faceC, distance);
            if (intersects && distance <= maxDistance) {
                return true;
            }
        }
        return false;
    }
}