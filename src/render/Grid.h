#pragma once

#include "Util.h"
#include "render/Primitives.h"

#include <span>

namespace render
{
	// TODO rename to CellPos? TODO move to render::grid again and replace usage with using grid::GridPos ?
	struct GridPos {
		uint8_t x;
		uint8_t y;

		auto operator<=>(const GridPos&) const = default;

		struct Hash
		{
			size_t operator()(const GridPos& key) const
			{
				size_t res = 0;
				::util::hashCombine(res, key.x);
				::util::hashCombine(res, key.y);
				return res;
			}
		};
	};
	inline std::ostream& operator <<(std::ostream& os, const GridPos& that)
	{
		return os << "[X=" << that.x << " Y=" << that.y << "]";
	}
} namespace std { template <> struct hash<render::GridPos> : render::GridPos::Hash {}; } namespace render {

	namespace grid
	{
		struct CellInfo {
			bool isInUse = false;
			DirectX::BoundingBox bbox;

			void mergeBbox(DirectX::BoundingBox otherBbox)
			{
				if (isInUse) {
					DirectX::BoundingBox::CreateMerged(bbox, bbox, otherBbox);
				}
				else {
					bbox = otherBbox;
					isInUse = true;
				}
			}
		};

		struct Grid;

		template<typename T, typename L>
		struct LayeredCells {
			std::vector<T> base;
			std::vector<L> layer;

			void resize(const Grid& grid);
		};

		struct Grid {
			Vec2 boundsMin, boundsMax;
			float distance;
			uint16_t cellCount;
			uint8_t cellCountXY;
			uint8_t groupSize = 0;
			uint8_t groupSizeXY = 0;

			grid::LayeredCells<grid::CellInfo, grid::CellInfo> cells;
		};

		template<typename T, typename L>
		void LayeredCells<T, L>::resize(const Grid& grid)
		{
			base.resize(grid.cellCount);
			if (grid.groupSize > 0) {
				layer.resize(grid.cellCount / grid.groupSize);
			}
		}

		Grid init(Vec2 boundsMin, Vec2 boundsMax, float scale, uint32_t faceCount);
		GridPos getGridPosForPoint(const Grid& grid, Vec2 point);
		uint16_t getIndex(GridPos pos);
		void updateBounds(Grid& grid, GridPos pos, const DirectX::BoundingBox& bbox);
		void propagateBoundsToLayer(Grid& grid);
	}
}