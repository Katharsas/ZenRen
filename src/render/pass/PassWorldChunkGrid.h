#pragma once

#include "../Common.h"

#include "directxcollision.h"

namespace render::pass::world::chunkgrid
{
	struct ChunkCameraInfo {
		bool intersectsFrustum = false;
		float distanceToCenterSq = 0;
	};

	template <VERTEX_FEATURE F>
	void updateSize(const MatToChunksToVerts<F>& meshData);
	std::pair<ChunkIndex, ChunkIndex> getIndexMinMax();
	uint32_t finalizeSize();
	template <VERTEX_FEATURE F>
	void updateMesh(const MatToChunksToVerts<F>& meshDataVariants);
	void updateCamera(const DirectX::BoundingFrustum& cameraFrustum);
	ChunkCameraInfo getCameraInfo(const ChunkIndex& index);
}