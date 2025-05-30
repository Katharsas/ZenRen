#pragma once

#include "render/Loader.h"

#include <octree.h>

namespace assets
{
	// Primitives.
	using OrthoVector3D = OrthoTree::VectorND<3, float>;
	using OrthoBoundingBox3D = OrthoTree::BoundingBoxND<3, float>;

	// We are using container variant (TreeBoxContainerND instead of TreeBoxND) so that bounding boxes are stored directly in tree and
	// we don't have to provide them separately on each search by storing them in SpatialCache.
	using OrthoOctree = OrthoTree::TreeBoxContainerND<3, 2, float>;

	struct VertLookupTree {
		std::unordered_map<uint32_t, render::VertKey> bboxIndexToVert;
		OrthoOctree tree;

		const std::vector<render::VertKey> bboxIdsToVertIds(const std::vector<size_t>& bboxIds) const  {
			std::vector<render::VertKey> result;
			for (auto id : bboxIds) {
				const auto& vertKey = this->bboxIndexToVert.find(id)->second;
				result.push_back(vertKey);
			}
			return result;
		}
	};

	VertLookupTree createVertLookup(const render::MatToChunksToVertsBasic& meshData);
	std::vector<render::VertKey> rayDownIntersected(const VertLookupTree& lookup, const Vec3& pos, float searchSizeY);
	std::vector<render::VertKey> rayDownIntersectedNaive(const render::MatToChunksToVertsBasic& meshData, const Vec3& pos, float searchSizeY);
	std::vector<render::VertKey> rayIntersected(const VertLookupTree& lookup, const DirectX::XMVECTOR& rayPosStart, const DirectX::XMVECTOR& rayPosEnd);

	struct LightLookupTree {
		OrthoOctree tree;
	};

	LightLookupTree createLightLookup(const std::vector<render::Light>& lights);

	struct FaceLookupContext
	{
		assets::VertLookupTree spatialTree;
		const render::MatToChunksToVertsBasic& data;
	};

	struct LightLookupContext
	{
		assets::LightLookupTree spatialTree;
		const std::vector<render::Light>& data;
	};
}

