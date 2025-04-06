#include "stdafx.h"
#include "DebugMeshes.h"

#include "AssetCache.h"
#include "render/MeshUtil.h"

namespace assets
{
    using namespace render;
    using std::vector;
    using std::array;
    using std::unordered_map;
    using DirectX::XMVECTOR;

    array<XMVECTOR, 3> posToXM4(const array<Vec3, 3>& face) {
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
    vector<array<XMVECTOR, 3>> createBboxVerts(const Vec3& posMin, const Vec3& posMax, const DirectX::XMMATRIX& transform)
    {
        // clockwise from below
        Vec3 quadA1 = { posMin.x, posMin.y, posMin.z };
        Vec3 quadB1 = { posMax.x, posMin.y, posMin.z };
        Vec3 quadC1 = { posMax.x, posMin.y, posMax.z };
        Vec3 quadD1 = { posMin.x, posMin.y, posMax.z };

        // clockwise from below
        Vec3 quadA2 = { posMin.x, posMax.y, posMin.z };
        Vec3 quadB2 = { posMax.x, posMax.y, posMin.z };
        Vec3 quadC2 = { posMax.x, posMax.y, posMax.z };
        Vec3 quadD2 = { posMin.x, posMax.y, posMax.z };

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

    void loadInstanceMeshBboxDebugVisual(MatToChunksToVertsBasic& target, const StaticInstance& instance)
    {
        vector<VertexPos> facesPos;
        vector<VertexBasic> facesOther;
        const auto& bboxFacesXm = createBboxVerts(toVec3(instance.bbox[0]), toVec3(instance.bbox[1]), instance.transform);
        for (auto& posXm : bboxFacesXm) {
            const auto faceNormal = toVec3(calcFlatFaceNormal(posXm));
            for (int32_t i = 0; i < 3; i++) {
                facesPos.push_back(toVec3(posXm[i]));
                facesOther.push_back({
                    faceNormal,
                    Uv { 0, 0 },
                    Uvi { 0, 0, -1 },
                    Color(1, 0, 0, 1)
                    });
            }
        }
        const Material defaultMat = { getTexId("bbox.tga") };
        insert(target, defaultMat, facesPos, facesOther);
    }

    vector<array<XMVECTOR, 3>> createDebugPointVerts(const Vec3& pos, const Vec3& scale)
    {
        float quadSize = 0.15f * scale.z;
        float minSize = 0.6f;
        Vec3 topA = { pos.x - quadSize, pos.y + std::max(minSize, scale.y), pos.z - quadSize };
        Vec3 topB = { pos.x - quadSize, pos.y + std::max(minSize, scale.y), pos.z + quadSize };
        Vec3 topC = { pos.x + quadSize, pos.y + std::max(minSize, scale.y), pos.z + quadSize };
        Vec3 topD = { pos.x + quadSize, pos.y + std::max(minSize, scale.y), pos.z - quadSize };

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

    void loadPointDebugVisual(MatToChunksToVertsBasic& target, const Vec3& pos, const Vec3& scale, const Color& color)
    {
        vector<VertexPos> facesPos;
        vector<VertexBasic> facesOther;
        const auto& bboxFacesXm = createDebugPointVerts(pos, scale);
        for (auto& posXm : bboxFacesXm) {
            const auto faceNormal = toVec3(calcFlatFaceNormal(posXm));
            for (int32_t i = 0; i < 3; i++) {
                facesPos.push_back(toVec3(posXm[i]));
                facesOther.push_back({
                    faceNormal,
                    Uv { 0, 0 },
                    Uvi { 0, 0, -1 },
                    color
                    });
            }
        }
        const Material defaultMat = { getTexId("point.tga") };
        insert(target, defaultMat, facesPos, facesOther);
    }

    vector<array<XMVECTOR, 3>> createDebugLineVerts(const Vec3& posStart, const Vec3& posEnd, const float width)
    {
        float minWidth = 0.01f;
        float actualWidth = std::max(minWidth, width);
        Vec3 posStartTop = { posStart.x, posStart.y + actualWidth, posStart.z };
        Vec3 posEndTop = { posEnd.x, posEnd.y + actualWidth, posEnd.z };

        vector result = {
            posToXM4({ posStart, posEnd, posEndTop }),
            posToXM4({ posStart, posEndTop, posStartTop }),
            // make double sided
            posToXM4({ posStart, posStartTop, posEndTop }),
            posToXM4({ posStart, posEndTop, posEnd }),
        };
        return result;
    }

    void loadLineDebugVisual(MatToChunksToVertsBasic& target, const Vec3& posStart, Vec3& posEnd, const Color& color)
    {
        vector<VertexPos> facesPos;
        vector<VertexBasic> facesOther;
        const auto& bboxFacesXm = createDebugLineVerts(posStart, posEnd, 0.02f);
        for (auto& posXm : bboxFacesXm) {
            const auto faceNormal = toVec3(calcFlatFaceNormal(posXm));
            for (int32_t i = 0; i < 3; i++) {
                facesPos.push_back(toVec3(posXm[i]));
                facesOther.push_back({
                    faceNormal,
                    Uv { 0, 0 },
                    Uvi { 0, 0, -1 },
                    color
                    });
            }
        }
        const Material defaultMat = { getTexId("line.tga") };
        insert(target, defaultMat, facesPos, facesOther);
    }
}
