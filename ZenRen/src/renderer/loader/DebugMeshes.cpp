#include "stdafx.h"
#include "DebugMeshes.h"

#include "MeshUtil.h"

namespace renderer::loader
{
    using std::vector;
    using std::array;
    using std::unordered_map;
    using DirectX::XMVECTOR;

    array<XMVECTOR, 3> posToXM4(const array<VEC3, 3>& face)
    {
        array<XMVECTOR, 3> result;
        for (int32_t i = 0; i < 3; i++) {
            const auto& vert = face[i];
            result[i] = toXM4Pos(vert);
        }
        return result;
    }

    void insertDebugFaces(unordered_map<Material, VEC_VERTEX_DATA>& target, const vector<array<XMVECTOR, 3>>& faces, const Material& mat, const D3DXCOLOR& color)
    {
        VEC_VERTEX_DATA faceVerts;
        for (auto& face : faces) {
            const auto faceNormal = toVec3(DirectX::XMVectorScale(calcFlatFaceNormal(face), -1.f));// TODO why do we need to reverse normal??
            for (int32_t i = 0; i < 3; i++) {
                faceVerts.vecPos.push_back(toVec3(face[i]));
                VERTEX_OTHER other;
                other.normal = faceNormal;
                other.uvDiffuse = { 0, 0 };
                other.uvLightmap = { 0, 0, -1 };
                other.colLight = color;
                other.dirLight = { -100, -100, -100 };
                other.lightSun = 0;
                faceVerts.vecNormalUv.push_back(other);
            }
        }
        insert(target, mat, faceVerts.vecPos, faceVerts.vecNormalUv);
    }

    /**
     * Create bottom and top quad face (double-sided) of AABB for debug rendering.
     */
    vector<array<XMVECTOR, 3>> createBboxVerts(const VEC3& posMin, const VEC3& posMax, const DirectX::XMMATRIX& transform)
    {
        // clockwise from below
        VEC3 quadA1 = { posMin.x, posMin.y, posMin.z };
        VEC3 quadB1 = { posMax.x, posMin.y, posMin.z };
        VEC3 quadC1 = { posMax.x, posMin.y, posMax.z };
        VEC3 quadD1 = { posMin.x, posMin.y, posMax.z };

        // clockwise from below
        VEC3 quadA2 = { posMin.x, posMax.y, posMin.z };
        VEC3 quadB2 = { posMax.x, posMax.y, posMin.z };
        VEC3 quadC2 = { posMax.x, posMax.y, posMax.z };
        VEC3 quadD2 = { posMin.x, posMax.y, posMax.z };

        vector result = {
            // top and bottom quad
            posToXM4({ quadA1, quadB1, quadC1 }),
            posToXM4({ quadA1, quadC1, quadD1 }),
            posToXM4({ quadA2, quadD2, quadC2 }),
            posToXM4({ quadA2, quadC2, quadB2 }),
            // make double sided
            posToXM4({ quadA1, quadC1, quadB1 }),
            posToXM4({ quadA1, quadD1, quadC1 }),
            posToXM4({ quadA2, quadC2, quadD2 }),
            posToXM4({ quadA2, quadB2, quadC2 }),
        };
        return result;
    }

    void loadInstanceMeshBboxDebugVisual(unordered_map<Material, VEC_VERTEX_DATA>& target, const StaticInstance& instance)
    {
        const auto& bboxFacesXm = createBboxVerts(instance.bbox[0], instance.bbox[1], instance.transform);
        const Material mat = { "bbox.tga" };
        insertDebugFaces(target, bboxFacesXm, mat, D3DXCOLOR(1, 0, 0, 1));
    }

    vector<array<XMVECTOR, 3>> createDebugPointVerts(const VEC3& pos, const VEC3& scale)
    {
        float quadSize = 0.15f;
        float minSize = 0.6f;
        VEC3 topA = { pos.x - quadSize, pos.y + std::max(minSize, scale.y), pos.z - quadSize };
        VEC3 topB = { pos.x - quadSize, pos.y + std::max(minSize, scale.y), pos.z + quadSize };
        VEC3 topC = { pos.x + quadSize, pos.y + std::max(minSize, scale.y), pos.z + quadSize };
        VEC3 topD = { pos.x + quadSize, pos.y + std::max(minSize, scale.y), pos.z - quadSize };

        vector result = {
            // top
            posToXM4({ topA, topB, topD }),
            posToXM4({ topB, topC, topD }),
            // sides
            posToXM4({ pos, topB, topA }),
            posToXM4({ pos, topC, topB }),
            posToXM4({ pos, topD, topC }),
            posToXM4({ pos, topA, topD }),
        };
        return result;
    }

    void loadPointDebugVisual(unordered_map<Material, VEC_VERTEX_DATA>& target, const VEC3& pos, const VEC3& scale, const D3DXCOLOR& color)
    {
        const auto& bboxFacesXm = createDebugPointVerts(pos, scale);
        const Material mat = { "point.tga" };
        insertDebugFaces(target, bboxFacesXm, mat, color);
    }

    vector<array<XMVECTOR, 3>> createDebugLineFaces(const VEC3& posStart, const VEC3& posEnd, const float width)
    {
        float minWidth = 0.01f;
        float actualWidth = std::max(minWidth, width);
        VEC3 posStartTop = { posStart.x, posStart.y + actualWidth, posStart.z };
        VEC3 posEndTop = { posEnd.x, posEnd.y + actualWidth, posEnd.z };

        vector result = {
            posToXM4({ posStart, posEnd, posEndTop }),
            posToXM4({ posStart, posEndTop, posStartTop }),
            // make double sided
            posToXM4({ posStart, posStartTop, posEndTop }),
            posToXM4({ posStart, posEndTop, posEnd }),
        };
        return result;
    }

    void loadLineDebugVisual(unordered_map<Material, VEC_VERTEX_DATA>& target, const VEC3& posStart, VEC3& posEnd, const D3DXCOLOR& color)
    {
        const auto& bboxFacesXm = createDebugLineFaces(posStart, posEnd, 0.02f);
        const Material mat = { "line.tga" };
        insertDebugFaces(target, bboxFacesXm, mat, color);
    }
}
