#include "stdafx.h"
#include "DebugMeshes.h"

#include "render/MeshUtil.h"

namespace assets
{
    using namespace render;
    using std::vector;
    using std::array;
    using std::unordered_map;
    using DirectX::XMVECTOR;

    array<XMVECTOR, 3> posToXM4(const array<VEC3, 3>& face) {
        array<XMVECTOR, 3> result;
        for (int32_t i = 0; i < 3; i++) {
            const auto& vert = face[i];
            result[i] = toXM4Pos(vert);
        }
        return result;
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
        vector<VERTEX_POS> facesPos;
        vector<VERTEX_OTHER> facesOther;
        const auto& bboxFacesXm = createBboxVerts(instance.bbox[0], instance.bbox[1], instance.transform);
        for (auto& posXm : bboxFacesXm) {
            const auto faceNormal = toVec3(calcFlatFaceNormal(posXm));
            for (int32_t i = 0; i < 3; i++) {
                facesPos.push_back(toVec3(posXm[i]));
                facesOther.push_back({
                    faceNormal,
                    UV { 0, 0 },
                    ARRAY_UV { 0, 0, -1 },
                    D3DXCOLOR(1, 0, 0, 1)
                    });
            }
        }
        const Material defaultMat = { "bbox.tga" };
        insert(target, defaultMat, facesPos, facesOther);
    }

    vector<array<XMVECTOR, 3>> createDebugPointVerts(const VEC3& pos, const VEC3& scale)
    {
        float quadSize = 0.15f * scale.z;
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
        vector<VERTEX_POS> facesPos;
        vector<VERTEX_OTHER> facesOther;
        const auto& bboxFacesXm = createDebugPointVerts(pos, scale);
        for (auto& posXm : bboxFacesXm) {
            const auto faceNormal = toVec3(calcFlatFaceNormal(posXm));
            for (int32_t i = 0; i < 3; i++) {
                facesPos.push_back(toVec3(posXm[i]));
                facesOther.push_back({
                    faceNormal,
                    UV { 0, 0 },
                    ARRAY_UV { 0, 0, -1 },
                    color
                    });
            }
        }
        const Material defaultMat = { "point.tga" };
        insert(target, defaultMat, facesPos, facesOther);
    }

    vector<array<XMVECTOR, 3>> createDebugLineVerts(const VEC3& posStart, const VEC3& posEnd, const float width)
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
        vector<VERTEX_POS> facesPos;
        vector<VERTEX_OTHER> facesOther;
        const auto& bboxFacesXm = createDebugLineVerts(posStart, posEnd, 0.02f);
        for (auto& posXm : bboxFacesXm) {
            const auto faceNormal = toVec3(calcFlatFaceNormal(posXm));
            for (int32_t i = 0; i < 3; i++) {
                facesPos.push_back(toVec3(posXm[i]));
                facesOther.push_back({
                    faceNormal,
                    UV { 0, 0 },
                    ARRAY_UV { 0, 0, -1 },
                    color
                    });
            }
        }
        const Material defaultMat = { "line.tga" };
        insert(target, defaultMat, facesPos, facesOther);
    }
}
