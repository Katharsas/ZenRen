#pragma once

#include "../Common.h"

#include "directxcollision.h"

namespace render::pass::world::chunkgrid
{
	template <VERTEX_FEATURE F>
	void updateSize(const MatToChunksToVerts<F>& meshData);
	uint32_t finalizeSize();
	template <VERTEX_FEATURE F>
	void updateMesh(const MatToChunksToVerts<F>& meshDataVariants);
	void updateCamera(const DirectX::BoundingFrustum& cameraFrustum);
	bool intersectsCamera(const ChunkIndex& index);
	std::pair<ChunkIndex, ChunkIndex> getIndexMinMax();
}