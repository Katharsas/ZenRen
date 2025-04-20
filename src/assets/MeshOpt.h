#pragma once

#include "../lib/meshoptimizer/src/meshoptimizer.h"// TODO wtf??

/**
 * @brief Wrap meshopt's C-API for in-place std::vector modification.
 *
 * Used to either generate entirely new indices, resulting in reduced vertex feature buffer sizes.
 * Or to optimize existing buffers, resulting in unchanged index and vertex feature buffer sizes.
 */
namespace assets::meshopt
{
    struct Remap {
        /**
         * @brief Maps from every previous vertex location to every new vertex location,
         *        so size must be equivalent to vertex count before remapping.
         */
        std::vector<uint32_t> vertMap;
        uint32_t vertCountRemapped;
    };

    template<typename T>
    void remapVertexBuffer(std::vector<T>& verts, const Remap& remap)
    {
        assert(verts.size() >= remap.vertCountRemapped);
        meshopt_remapVertexBuffer(
            verts.data(), verts.data(), verts.size(), sizeof(T), remap.vertMap.data());
        verts.resize(remap.vertCountRemapped);
    }

    void remapIndexBuffer(std::vector<VertexIndex>& indices, const Remap& remap)
    {
        assert(indices.size() >= remap.vertMap.size());
        meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.vertMap.data());
    }

    /**
     * @param remap - size must be equal to number of indices / unindexed verts
     */
    std::vector<VertexIndex> createIndexBuffer(const Remap& remap)
    {
        std::vector<VertexIndex> indices(remap.vertMap.size());
        meshopt_remapIndexBuffer(indices.data(), nullptr, remap.vertMap.size(), remap.vertMap.data());
        return indices;
    }

    template <uint32_t N>
    Remap generateIndicesRemap(uint32_t unindexedVertexCount, std::array<meshopt_Stream, N> vertexStreams)
    {
        Remap remap{
            .vertMap = std::vector<uint32_t>(unindexedVertexCount),
        };
        remap.vertCountRemapped = meshopt_generateVertexRemapMulti(
            remap.vertMap.data(), nullptr, unindexedVertexCount, unindexedVertexCount, vertexStreams.data(), vertexStreams.size());

        return remap;
    }

    template <typename T>
    meshopt_Stream createStream(const std::vector<T>& vec)
    {
        return meshopt_Stream{ vec.data(), sizeof(T), sizeof(T) };
    }

    void optimizeVertexCache(std::vector<VertexIndex>& indices, uint32_t vertexCount)
    {
        meshopt_optimizeVertexCache(
            indices.data(), indices.data(), indices.size(), vertexCount);
    }

    template<typename T>
    void optimizeOverdraw(std::vector<VertexIndex>& indices, const std::vector<T>& vertsWithPosAtStart)
    {
        assert(sizeof(T) >= 12);
        const float* vertPosStart = (float*)vertsWithPosAtStart.data();
        meshopt_optimizeOverdraw(
            indices.data(), indices.data(), indices.size(), vertPosStart, vertsWithPosAtStart.size(), sizeof(T), 1.05f);
    }

    Remap optimizeVertexFetchRemap(std::vector<VertexIndex>& indices, uint32_t vertexCount)
    {
        Remap remap{
            .vertMap = std::vector<uint32_t>(vertexCount),
            .vertCountRemapped = vertexCount
        };
        uint32_t remapSize = meshopt_optimizeVertexFetchRemap(
            remap.vertMap.data(), indices.data(), indices.size(), vertexCount);

        assert(remapSize == vertexCount);
        return remap;
    }
}