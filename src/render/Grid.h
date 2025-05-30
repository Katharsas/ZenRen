#pragma once

#include "Util.h"
#include "render/Primitives.h"

namespace render
{
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

	struct Grid {
		Vec2 boundsMin, boundsMax;
		float distance;
		uint8_t cellsPerDim;
		uint8_t cellsPerDimMain;
		uint8_t cellsPerDimSub;
	};

	namespace grid
	{
		Grid init(Vec2 boundsMin, Vec2 boundsMax, float scale, uint32_t faceCount);

		GridPos getGridPosForPoint(const Grid& grid, Vec2 point);
	}
}