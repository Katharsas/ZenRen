#include "stdafx.h"
#include "LookupTrees.h"

#include "render/basic/MeshUtil.h"

#include "bvh/v2/vec.h"
#include "bvh/v2/ray.h"
#include "bvh/v2/node.h"
#include "bvh/v2/default_builder.h"
#include "bvh/v2/thread_pool.h"
#include "bvh/v2/executor.h"
#include "bvh/v2/stack.h"

namespace assets
{
    using namespace render;
    using ::std::vector;
    using ::std::unordered_map;
    using ::std::array;
    using DirectX::XMVECTOR;

    const float rayIntersectTolerance = 0.001f;


    OrthoVector3D toOrthoVec3(const XMVECTOR& xm4)
    {
        DirectX::XMFLOAT4 result;
        DirectX::XMStoreFloat4(&result, xm4);
        return { result.x, result.y, result.z };
    }

    OrthoBoundingBox3D createBB3D(const array<VertexPos, 3>& verts)
    {
        float minX =  FLT_MAX, minY =  FLT_MAX, minZ =  FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
        for (uint32_t i = 0; i < 3; i++) {
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

    /*
    VertLookupTree createVertLookup(const MatToChunksToVertsBasic& meshData)
    {
        // TODO bboxes for each face could be created during mesh loading ideally (?)
        vector<OrthoBoundingBox3D> bboxes;

        VertLookupTree result;
        uint32_t bbIndex = 0;

        forEachFace(meshData, [&](const VertKey& vertKey) -> void {
            bboxes.push_back(createBB3D(vertKey.getPos(meshData)));
            result.bboxIndexToVert.insert({ bbIndex, vertKey });
            bbIndex++;
        });

        result.tree = OrthoOctree(bboxes
            , 8 // max depth
            , std::nullopt // user-provided bounding Box for all
            , 10 // max element in a node; 10 works well for us and has been observed to work well in general according to lib author (Github)
        );
        return result;
	}

    vector<VertKey> rayDownIntersected(const VertLookupTree& lookup, const Vec3& pos, float searchSizeY)
    {
        auto intersectedBoxes = lookup.tree.RayIntersectedAll({ pos.x, pos.y, pos.z }, { 0, -1, 0 }, rayIntersectTolerance, searchSizeY);
        return lookup.bboxIdsToVertIds(intersectedBoxes);
    }

    vector<VertKey> rayIntersected(const VertLookupTree& lookup, const XMVECTOR& rayPosStart, const XMVECTOR& rayPosEnd)
    {
        XMVECTOR dirXm = DirectX::XMVectorSubtract(rayPosEnd, rayPosStart);
        float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(dirXm));
        auto intersectedBoxes = lookup.tree.RayIntersectedAll(toOrthoVec3(rayPosStart), toOrthoVec3(dirXm), rayIntersectTolerance, length);

        return lookup.bboxIdsToVertIds(intersectedBoxes);
    }

    */

    // ##############################################################################################################################
    // Naive implementation does quadratic looping over all world faces each time, no spatial structure used. For debugging purposes.
    // ##############################################################################################################################

    inline bool rayDownIntersectsFaceBB(const Vec3& pos, const array<VertexPos, 3>& verts, const float searchSizeY)
    {
        float minX =  FLT_MAX, minY =  FLT_MAX, minZ =  FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

        for (uint32_t i = 0; i < 3; i++) {
            const auto& vert = verts[i];
            minX = std::min(minX, vert.x);
            maxX = std::max(maxX, vert.x);
        }
        if (pos.x >= (maxX + rayIntersectTolerance) || pos.x <= (minX - rayIntersectTolerance)) return false;

        for (uint32_t i = 0; i < 3; i++) {
            const auto& vert = verts[i];
            minZ = std::min(minZ, vert.z);
            maxZ = std::max(maxZ, vert.z);
        }
        if (pos.z >= (maxZ + rayIntersectTolerance) || pos.z <= (minZ - rayIntersectTolerance)) return false;

        for (uint32_t i = 0; i < 3; i++) {
            const auto& vert = verts[i];
            minY = std::min(minY, vert.y);
            maxY = std::max(maxY, vert.y);
        }
        return !(pos.y >= (maxY + rayIntersectTolerance + searchSizeY) || pos.y >= (minY - rayIntersectTolerance));
    }

    std::vector<VertKey> rayDownIntersectedNaive(const MatToChunksToVertsBasic& meshData, const Vec3& pos, float searchSizeY)
    {
        vector<VertKey> result;
        forEachFace(meshData, [&](const VertKey& vertKey) -> void {
            if (rayDownIntersectsFaceBB(pos, vertKey.getPos(meshData), searchSizeY)) {
                result.push_back(vertKey);
            }
        });
        return result;
    }

    // ##############################################################################################################################
    // Lights
    // ##############################################################################################################################

    LightLookupTree createLightLookup(const vector<Light>& lights)
    {
        vector<OrthoBoundingBox3D> bboxes;
        for (auto& light : lights) {
            auto pos = light.pos;
            auto range = light.range;
            bboxes.push_back({
                {pos.x - range, pos.y - range, pos.z - range},
                {pos.x + range, pos.y + range, pos.z + range}
                });
        }
        LightLookupTree result;
        result.tree = OrthoOctree(bboxes
            , 8 // max depth
            , std::nullopt // user-provided bounding Box for all
            , 10 // max element in a node; 10 works well for us and has been observed to work well in general according to lib author (Github)
        );
        return result;
    }

    using BvhVec3 = bvh::v2::Vec<float, 3>;
    using BvhBBox = bvh::v2::BBox<float, 3>;
    using BvhTri = bvh::v2::Tri<float, 3>;
    using BvhRay = bvh::v2::Ray<float, 3>;

    BvhVec3 toBvhVec3(const XMVECTOR& xm4)
    {
        DirectX::XMFLOAT4 result;
        DirectX::XMStoreFloat4(&result, xm4);
        return { result.x, result.y, result.z };
    }

    // Permuting the primitive data allows to remove indirections during traversal, which makes it faster.
    static constexpr bool should_permute = false;

    VertLookupTree createVertLookup2(const MatToChunksToVertsBasic& meshData)
    {
        VertLookupTree result;
        std::vector<BvhTri> tris;
        
        uint32_t index = 0;
        forEachFace(meshData, [&](const VertKey& vertKey) -> void {
            const auto& verts = vertKey.getPos(meshData);
            tris.push_back({
                { verts[0].x, verts[0].y, verts[0].z },
                { verts[1].x, verts[1].y, verts[1].z },
                { verts[2].x, verts[2].y, verts[2].z },
            });
            result.treeIndexToVert.insert({ index, vertKey });
            index++;
        });

        bvh::v2::ThreadPool thread_pool;
        bvh::v2::ParallelExecutor executor(thread_pool);

        std::vector<BvhBBox> bboxes;
        std::vector<BvhVec3> centers;
        executor.for_each(0, tris.size(), [&](size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                bboxes[i] = tris[i].get_bbox();
                centers[i] = tris[i].get_center();
            }
        });

        typename bvh::v2::DefaultBuilder<BvhNode>::Config config;
        config.quality = bvh::v2::DefaultBuilder<BvhNode>::Quality::Medium;
        result.bvh = bvh::v2::DefaultBuilder<BvhNode>::build(thread_pool, bboxes, centers, config);

        result.precomputed.resize(tris.size());
        executor.for_each(0, tris.size(), [&](size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                auto j = should_permute ? result.bvh.prim_ids[i] : i;
                result.precomputed[i] = tris[j];
            }
        });

        return result;
    }

    std::optional<VertLookupResult> rayIntersected(const VertLookupTree& lookup, BvhVec3 rayOrigin, BvhVec3 rayDir, float rayMaxLength)
    {
        auto ray = BvhRay {
            rayOrigin,
            rayDir,
            0,
            rayMaxLength
        };

        static constexpr size_t invalidId = std::numeric_limits<size_t>::max();
        static constexpr size_t stackSize = 64;
        static constexpr bool useRobustTraversal = false;

        auto hitId = invalidId;
        Uv hitPoint;

        // Traverse the BVH and get the u, v coordinates of the closest intersection.
        bvh::v2::SmallStack<Bvh::Index, stackSize> stack;
        lookup.bvh.intersect<false, useRobustTraversal>(ray, lookup.bvh.get_root().index, stack,
            [&](size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                size_t j = should_permute ? i : lookup.bvh.prim_ids[i];
                if (auto hit = lookup.precomputed[j].intersect(ray)) {
                    hitId = i;
                    std::tie(ray.tmax, hitPoint.u, hitPoint.v) = *hit;
                }
            }
            return hitId != invalidId;
        });

        if (hitId != invalidId) {
            return VertLookupResult{
                .vertKey = lookup.treeIndexToVert.at(hitId),
                .hitPoint = hitPoint,
                .hitDistance = ray.tmax,
            };
        }
        else {
            return std::nullopt;
        }
    }

    std::optional<VertLookupResult> rayDownIntersected(const VertLookupTree& lookup, const Vec3& pos, float searchSizeY)
    {
        return rayIntersected(lookup, BvhVec3 { pos.x, pos.y, pos.z }, { 0, -1, 0 }, searchSizeY);
    }

    std::optional<VertLookupResult> rayIntersected(const VertLookupTree& lookup, const XMVECTOR& rayPosStart, const XMVECTOR& rayPosEnd)
    {
        auto dirXm = DirectX::XMVectorSubtract(rayPosEnd, rayPosStart);
        auto lengthXm = DirectX::XMVector3LengthEst(dirXm);
        float length = DirectX::XMVectorGetX(lengthXm);

        return rayIntersected(lookup, toBvhVec3(rayPosStart), toBvhVec3(DirectX::XMVectorDivide(dirXm, lengthXm)), length);
    }
}
