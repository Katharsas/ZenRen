#include "stdafx.h"
#include "VertPosLookup.h"

namespace renderer::loader
{
    using ::std::vector;
    using ::std::unordered_map;

    OrthoBoundingBox3D createBB3D(const vector<VERTEX_POS>& verts, uint32_t vertIndex)
    {
        float minX =  FLT_MAX, minY =  FLT_MAX, minZ =  FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
        //float minX = FLT_MAX, minZ = FLT_MAX;
        //float maxX = -FLT_MAX, maxZ = -FLT_MAX;
        for (uint32_t i = vertIndex; i < vertIndex + 3; i++) {
            auto& vert = verts[i];
            minX = std::min(minX, vert.x);
            minY = std::min(minY, vert.y);
            minZ = std::min(minZ, vert.z);
            maxX = std::max(maxX, vert.x);
            maxY = std::max(maxY, vert.y);
            maxZ = std::max(maxZ, vert.z);
        }
        return  OrthoTree::BoundingBoxND<3, float>{ {minX, minY, minZ}, {maxX, maxY, maxZ} };
    }

    SpatialCache createSpatialCache(const unordered_map<Material, VEC_VERTEX_DATA>& meshData)
    {
        vector<OrthoBoundingBox3D> bboxes;

        SpatialCache result;
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

    vector<VertKey> lookupVertsAtPos(const SpatialCache& cache, const VEC3& pos, float searchSizeY)
    {
        float searchSize = 0.1f;
        auto searchBox = OrthoBoundingBox3D{
            { pos.x - searchSize, pos.y - searchSizeY, pos.z - searchSize },
            { pos.x + searchSize, pos.y + searchSizeY, pos.z + searchSize }
        };

        constexpr bool shouldFullyContain = false;
        auto insideBoxIds = cache.tree.RangeSearch<shouldFullyContain>(searchBox);

        vector<VertKey> result;
        for (auto id : insideBoxIds) {
            const auto& vertKey = cache.bboxIndexToVert.find(id)->second;
            result.push_back(vertKey);
        }
        return result;
    }

    // ##############################################################################################################################
    // Naive implementation does quadratic looping over all world faces each time, so spatial structure used. For debugging purposes.
    // ##############################################################################################################################

    __inline bool intersectsBbox2DNaive(const VEC3& pos, const vector<VERTEX_POS>& verts, const size_t vertIndex) {
        float minX = FLT_MAX, minZ = FLT_MAX;
        float maxX = -FLT_MAX, maxZ = -FLT_MAX;
        for (int i = vertIndex; i < vertIndex + 3; i++) {
            auto& vert = verts[i];
            minX = std::min(minX, vert.x);
            minZ = std::min(minZ, vert.z);
            maxX = std::max(maxX, vert.x);
            maxZ = std::max(maxZ, vert.z);
        }
        return true
            && pos.x < maxX&& pos.x > minX
            && pos.z < maxZ&& pos.z > minZ;
    }

    std::vector<VertKey> lookupVertsAtPosNaive(const unordered_map<Material, VEC_VERTEX_DATA>& meshData, const VEC3& pos)
    {
        vector<VertKey> result;
        for (auto& it : meshData) {
            auto& vecPos = it.second.vecPos;
            for (uint32_t i = 0; i < vecPos.size(); i += 3) {
                if (intersectsBbox2DNaive(pos, vecPos, i)) {
                    result.push_back({ &it.first, i });
                }
            }
        }
        return result;
    }
}
