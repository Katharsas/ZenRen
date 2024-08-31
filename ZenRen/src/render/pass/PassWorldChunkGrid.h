#pragma once

#include "../Common.h"

#include "directxcollision.h"

namespace render::pass::world::chunkgrid
{
	void updateSize(const VERT_CHUNKS_BY_MAT& meshData);
	uint32_t finalizeSize();
	void updateMesh(const VERT_CHUNKS_BY_MAT& meshData);
	void updateCamera(const DirectX::BoundingFrustum& cameraFrustum);
	bool intersectsCamera(const ChunkIndex& index);
}