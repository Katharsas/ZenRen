#pragma once

#include "../RendererCommon.h"

#include "octree_attcs/octree.h"

namespace renderer::loader
{
	// BoundingBox primitive.
	using OrthoBoundingBox3D = OrthoTree::BoundingBoxND<3, float>;

	// We are using container variant (TreeBoxContainerND instead of TreeBoxND) so that bounding boxes are stored directly in tree and
	// we don't have to provide them separately on each search by storing them in SpatialCache.
	using OrthoOctree = OrthoTree::TreeBoxContainerND<3, 2, float>;

	struct VertLookupTree {
		std::unordered_map<uint32_t, VertKey> bboxIndexToVert;
		OrthoOctree tree;
	};

	VertLookupTree createVertLookup(const std::unordered_map<Material, VEC_VERTEX_DATA>& meshData);
	std::vector<VertKey> rayDownIntersected(const VertLookupTree& lookup, const VEC3& pos, float searchSizeY);
	std::vector<VertKey> rayDownIntersectedNaive(const std::unordered_map<Material, VEC_VERTEX_DATA>& meshData, const VEC3& pos, float searchSizeY);
}

