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

    using BvhVec3 = bvh::v2::Vec<float, 3>;

    BvhBBox createBbox(const array<VertexPos, 3>& verts)
    {
        float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
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
        return BvhBBox { {minX, minY, minZ}, { maxX, maxY, maxZ } };
    }

    BvhVec3 createTriCenter(const array<VertexPos, 3>& verts)
    {
        Vec3 center = mul(add(add(verts[0], verts[1]), verts[2]), 1.f / 3.f);
        return BvhVec3 { center.x , center.y, center.z };
    }

    bvh::v2::ThreadPool thread_pool;
    bvh::v2::ParallelExecutor executor(thread_pool);
    
    VertLookupTree createVertLookup(const MatToChunksToVertsBasic& meshData)
    {
        // TODO bboxes for each face could be created during mesh loading ideally (?)

        VertLookupTree result;
        uint32_t treeIndex = 0;

        //std::vector<BvhBBox> bboxes;
        std::vector<BvhVec3> centers;

        forEachFace(meshData, [&](const VertKey& vertKey) -> void {
            auto tri = vertKey.getPos(meshData);
            centers.push_back(createTriCenter(tri));
            result.bboxes.push_back(createBbox(tri));
            result.treeIndexToVert.insert({ treeIndex, vertKey });
            treeIndex++;
        });

        typename bvh::v2::DefaultBuilder<BvhNode>::Config config;
        config.quality = bvh::v2::DefaultBuilder<BvhNode>::Quality::High;
        result.bvh = bvh::v2::DefaultBuilder<BvhNode>::build(thread_pool, result.bboxes, centers, config);

        return result;
	}

    /*
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

    //using BvhVec3 = bvh::v2::Vec<float, 3>;
    //using BvhBBox = bvh::v2::BBox<float, 3>;
    //using BvhTri = bvh::v2::Tri<float, 3>;
    //using BvhRay = bvh::v2::Ray<float, 3>;

    BvhVec3 toBvhVec3(const XMVECTOR& xm4)
    {
        DirectX::XMFLOAT4 result;
        DirectX::XMStoreFloat4(&result, xm4);
        return { result.x, result.y, result.z };
    }

    // Permuting the primitive data allows to remove indirections during traversal, which makes it faster.
    static constexpr bool should_permute = false;

    vector<VertKey> rayIntersected(const VertLookupTree& lookup, BvhVec3 rayOrigin, BvhVec3 rayDir, float rayMaxLength)
    {
        using BvhRay = bvh::v2::Ray<float, 3>;
        auto ray = BvhRay {
            rayOrigin,
            rayDir,
            0,
            rayMaxLength
        };

        static constexpr size_t invalidId = std::numeric_limits<size_t>::max();
        static constexpr size_t stackSize = 64;
        static constexpr bool useRobustTraversal = false;

        vector<VertKey> result;

        auto inv_dir = ray.template get_inv_dir<!useRobustTraversal>();
        auto inv_org = -inv_dir * ray.org;
        auto inv_dir_pad = ray.pad_inv_dir(inv_dir);
        auto octant = ray.get_octant();

        bvh::v2::SmallStack<Bvh::Index, stackSize> stack;
        lookup.bvh.intersect<false, useRobustTraversal>(ray, lookup.bvh.get_root().index, stack,
            [&](size_t begin, size_t end) {
                //if (end - begin >= 5) {
                //    LOG(INFO) << "Leaf Tris: " << std::to_string(end - begin);
                //}
                
                for (size_t i = begin; i < end; ++i) {
                    BvhBBox bbox = lookup.bboxes.at(i);
                    BvhVec3 minBounds;
                    BvhVec3 maxBounds;
                    for (size_t j = 0; j < 3; ++j) {
                        minBounds[j] = octant[j] ? bbox.max[j] : bbox.min[j];
                        maxBounds[j] = octant[j] ? bbox.min[j] : bbox.max[j];
                    }
                    auto tmin = bvh::v2::fast_mul_add(minBounds, inv_dir, inv_org);
                    auto tmax = bvh::v2::fast_mul_add(maxBounds, inv_dir, inv_org);

                    auto t0 = std::max(std::max(tmin[0], tmin[1]), std::max(tmin[2], ray.tmin));
                    auto t1 = std::min(std::min(tmax[0], tmax[1]), std::min(tmax[2], ray.tmax));
                    bool intersect =  t0 <= t1;

                    if (intersect) {
                        result.push_back(lookup.treeIndexToVert.at(i));
                        //if (result.size() % 1000 == 0) {
                        //    LOG(INFO) << "Result count: " << std::to_string(result.size());
                        //}
                    }
                }
                return true;
            }
        );

        return result;
    }

    vector<VertKey> rayDownIntersected(const VertLookupTree& lookup, const Vec3& pos, float searchSizeY)
    {
        return rayIntersected(lookup, BvhVec3 { pos.x, pos.y, pos.z }, { 0, -1, 0 }, searchSizeY);
    }

    vector<VertKey> rayIntersected(const VertLookupTree& lookup, const XMVECTOR& rayPosStart, const XMVECTOR& rayPosEnd)
    {
        auto dirXm = DirectX::XMVectorSubtract(rayPosEnd, rayPosStart);
        auto lengthXm = DirectX::XMVector3LengthEst(dirXm);
        float length = DirectX::XMVectorGetX(lengthXm);

        return rayIntersected(lookup, toBvhVec3(rayPosStart), toBvhVec3(DirectX::XMVectorDivide(dirXm, lengthXm)), length);
    }
}
