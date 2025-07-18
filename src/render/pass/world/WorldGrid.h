#pragma once

#include "render/Common.h"

#include "directxcollision.h"

namespace render::pass::world::chunkgrid
{
	struct GridIndex {
		uint16_t outerIndex;
		uint16_t innerIndex;
	};

	struct CellCameraInfo {
		bool intersectsFrustum = false;
		bool intersectsFrustumClose = false;
		float distanceCenter2dSq = 0;
		float distanceCornerNearSq = 0;
		float distanceCornerFarSq = 0;
	};

	struct LayerCellCameraInfo {
		DirectX::ContainmentType intersectFrustumType = DirectX::ContainmentType::DISJOINT;
		DirectX::ContainmentType intersectFrustumCloseType = DirectX::ContainmentType::DISJOINT;
	};

	uint16_t init(const grid::Grid& grid);
	std::pair<GridPos, GridPos> getIndexMinMax();
	void updateCamera(const DirectX::BoundingFrustum& cameraFrustum, bool updateCulling, bool updateCloseIntersect, float closeRadius, bool updateDistances);

	GridIndex getIndex(const GridPos& index);
	LayerCellCameraInfo getCameraInfoOuter(uint16_t outerIndex);
	CellCameraInfo getCameraInfoInner(uint16_t innerIndex);
	grid::CellInfo getCellInfoInner(uint16_t innerIndex);
}