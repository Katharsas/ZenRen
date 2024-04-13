#include "stdafx.h"
#include "VertPosLookup.h"

namespace renderer::loader
{
    using ::std::vector;
    using ::std::unordered_map;
    using ::std::array;

    const float rayIntersectTolerance = 0.0001f;

    OrthoBoundingBox3D createBB3D(const vector<VERTEX_POS>& verts, uint32_t vertIndex)
    {
        float minX =  FLT_MAX, minY =  FLT_MAX, minZ =  FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
        for (uint32_t i = vertIndex; i < vertIndex + 3; i++) {
            auto& vert = verts[i];
            minX = std::min(minX, vert.x);
            minY = std::min(minY, vert.y);
            minZ = std::min(minZ, vert.z);
            maxX = std::max(maxX, vert.x);
            maxY = std::max(maxY, vert.y);
            maxZ = std::max(maxZ, vert.z);
        }
        return OrthoTree::BoundingBoxND<3, float>{ {minX, minY, minZ}, {maxX, maxY, maxZ} };
    }

    OrthoBoundingBox3D createRaySearchBox(const array<VEC3, 2>& posStartEnd)
    {
        float minX =  FLT_MAX, minY =  FLT_MAX, minZ =  FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
        for (uint32_t i = 0; i < posStartEnd.size(); i++) {
            auto& vert = posStartEnd[i];
            minX = std::min(minX, vert.x);
            minY = std::min(minY, vert.y);
            minZ = std::min(minZ, vert.z);
            maxX = std::max(maxX, vert.x);
            maxY = std::max(maxY, vert.y);
            maxZ = std::max(maxZ, vert.z);
        }
        return OrthoTree::BoundingBoxND<3, float>{ {minX, minY, minZ}, { maxX, maxY, maxZ } };
    }

    VertLookupTree createVertLookup(const unordered_map<Material, VEC_VERTEX_DATA>& meshData)
    {
        vector<OrthoBoundingBox3D> bboxes;

        VertLookupTree result;
        uint32_t bbIndex = 0;
        for (auto& it : meshData) {
            auto& mat = it.first;
            auto& vecPos = it.second.vecPos;
            for (uint32_t i = 0; i < vecPos.size(); i += 3) {
                bboxes.push_back(createBB3D(vecPos, i));
                result.bboxIndexToVert.insert({ bbIndex, { &mat, i } });
                bbIndex++;
            }
        }
        result.tree = OrthoOctree(bboxes
            , 8 // max depth
            , std::nullopt // user-provided bounding Box for all
            , 10 // max element in a node; 10 works well for us and has been observed to work well in general according to lib author (Github)
        );
        return result;
	}

    vector<VertKey> rayDownIntersected(const VertLookupTree& lookup, const VEC3& pos, float searchSizeY)
    {
        auto searchBox = OrthoBoundingBox3D{
            { pos.x - rayIntersectTolerance, pos.y - searchSizeY, pos.z - rayIntersectTolerance },
            { pos.x + rayIntersectTolerance, pos.y,               pos.z + rayIntersectTolerance }
        };

        constexpr bool shouldFullyContain = false;
        auto intersectedBoxes = lookup.tree.RangeSearch<shouldFullyContain>(searchBox);

        // TODO while RayIntersectedAll should be equivalent from what i understand, it is missing most bboxes that RangeSearch finds
        //auto intersectedBoxes = lookup.tree.RayIntersectedAll({ pos.x, pos.y, pos.z }, { 0, -100.f, 0 }, rayIntersectTolerance, searchSizeY);

        return lookup.bboxIdsToVertIds(intersectedBoxes);
    }

    vector<VertKey> rayIntersected(const VertLookupTree& lookup, const VEC3& rayPosStart, const VEC3& rayPosEnd) {
        // TODO
        // RayIntersectedAll is not giving the expected results, until then just get all boxes that intersect the bbox of the ray.
        // This will be very slow for long rays but we just don't to long rays for now. We could also construct multiple bboxes along the ray.

        auto searchBox = createRaySearchBox({ rayPosStart, rayPosEnd });

        constexpr bool shouldFullyContain = false;
        auto intersectedBoxes = lookup.tree.RangeSearch<shouldFullyContain>(searchBox);

        return lookup.bboxIdsToVertIds(intersectedBoxes);
    }

    // ##############################################################################################################################
    // Naive implementation does quadratic looping over all world faces each time, no spatial structure used. For debugging purposes.
    // ##############################################################################################################################

    __inline bool rayDownIntersectsFaceBB(const VEC3& pos, const vector<VERTEX_POS>& verts, const size_t vertIndex, const float searchSizeY) {
        float minX =  FLT_MAX, minY =  FLT_MAX, minZ =  FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

        for (uint32_t i = vertIndex; i < vertIndex + 3; i++) {
            const auto& vert = verts[i];
            minX = std::min(minX, vert.x);
            maxX = std::max(maxX, vert.x);
        }
        if (pos.x >= (maxX + rayIntersectTolerance) || pos.x <= (minX - rayIntersectTolerance)) return false;

        for (uint32_t i = vertIndex; i < vertIndex + 3; i++) {
            const auto& vert = verts[i];
            minZ = std::min(minZ, vert.z);
            maxZ = std::max(maxZ, vert.z);
        }
        if (pos.z >= (maxZ + rayIntersectTolerance) || pos.z <= (minZ - rayIntersectTolerance)) return false;

        for (uint32_t i = vertIndex; i < vertIndex + 3; i++) {
            const auto& vert = verts[i];
            minY = std::min(minY, vert.y);
            maxY = std::max(maxY, vert.y);
        }
        return !(pos.y >= (maxY + rayIntersectTolerance + searchSizeY) || pos.y >= (minY - rayIntersectTolerance));
    }

    std::vector<VertKey> rayDownIntersectedNaive(const unordered_map<Material, VEC_VERTEX_DATA>& meshData, const VEC3& pos, float searchSizeY)
    {
        vector<VertKey> result;
        for (auto& it : meshData) {
            auto& vecPos = it.second.vecPos;
            for (uint32_t i = 0; i < vecPos.size(); i += 3) {
                if (rayDownIntersectsFaceBB(pos, vecPos, i, searchSizeY)) {
                    result.push_back({ &it.first, i });
                }
            }
        }
        return result;
    }
}