#pragma once

#include "../Common.h"

#include "directxcollision.h"

namespace render::pass::world::chunkgrid
{
	struct ChunkCameraInfo {
		bool intersectsFrustum = false;
		float distanceToCenterSq = 0;
	};

	uint16_t init(const Grid& grid);
	std::pair<GridPos, GridPos> getIndexMinMax();
	void updateCamera(const DirectX::BoundingFrustum& cameraFrustum);
	ChunkCameraInfo getCameraInfo(const GridPos& index);
}