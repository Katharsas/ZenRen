#include "stdafx.h"
#include "Grid.h"

namespace render::grid
{
	using namespace DirectX;
	using std::vector;

	// Adopted from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
	// Altertively, something like https://github.com/Forceflow/libmorton could be used, but seems overkill for small grids with few cells
	namespace morton
	{
		uint16_t part1By1(uint8_t x8)
		{
			// "Insert" a 0 bit before each of the 8 bits of x
			uint16_t x = x8;             // x = ---- ---- 7654 3210
			x = (x ^ (x << 4)) & 0x0f0f; // x = ---- 7654 ---- 3210
			x = (x ^ (x << 2)) & 0x3333; // x = --76 --54 --32 --10
			x = (x ^ (x << 1)) & 0x5555; // x = -7-6 -5-4 -3-2 -1-0
			return x;
		}
		uint8_t compact1By1(uint16_t x)
		{
			// Inverse of part1By1 - "delete" all odd-indexed bits
			x &= 0x5555;                 // x = -7-6 -5-4 -3-2 -1-0
			x = (x ^ (x >> 1)) & 0x3333; // x = --76 --54 --32 --10
			x = (x ^ (x >> 2)) & 0x0f0f; // x = ---- 7654 ---- 3210
			x = (x ^ (x >> 4)) & 0x00ff; // x = ---- ---- 7654 3210
			return (uint8_t)x;
		}

		uint16_t encode2d(uint8_t x, uint8_t y)
		{
			return (part1By1(y) << 1) + part1By1(x);
		}

		uint8_t decode2dX(uint16_t code)
		{
			return compact1By1(code);
		}
		uint8_t decode2dY(uint16_t code)
		{
			return compact1By1(code >> 1);
		}
	}

	constexpr float WORLD_FACE_DENSITY_G2 = 0.164;// G1: 0.046, G2: 0.164
	constexpr float CELLS_PER_AREA = 1.f / (60 * 60);// each cell should contain about 60*60 meters (at G2 fidelity)
	constexpr float FIDELITY_WEIGHT = 0.6;// how much estimated graphical fidelity will alter cell count

	Grid init(Vec2 boundsMin, Vec2 boundsMax, float scale, uint32_t faceCount)
	{
		assert(boundsMax.x > boundsMin.x);
		assert(boundsMax.y > boundsMin.y);

		Grid grid;
		float distX = boundsMax.x - boundsMin.x;
		float distY = boundsMax.y - boundsMin.y;
		grid.distance = std::max(distX, distY) * scale;

		// bounds
		float centerX = boundsMin.x + (distX * 0.5f);
		float centerY = boundsMin.y + (distY * 0.5f);
		float gridDistHalf = grid.distance * 0.5;
		grid.boundsMin = { centerX - gridDistHalf, centerY - gridDistHalf };
		grid.boundsMax = { centerX + gridDistHalf, centerY + gridDistHalf };

		// esitmated graphical fidelity (since we don't know anything about VOB fidelity)
		float faceDensity = faceCount / (distX * distY);
		float faceDensityNorm = faceDensity / WORLD_FACE_DENSITY_G2;
		float fidelityFactor = std::lerp(1.0f, faceDensityNorm, FIDELITY_WEIGHT);

		// determine appropriate number of cells with power of 2
		float cellsTarget = (grid.distance * grid.distance) * CELLS_PER_AREA * fidelityFactor;
		float cellsPerDimTarget = std::sqrt(cellsTarget);

		uint32_t cellsPerDimPowerOf2 = std::pow(2, (uint32_t) std::ceil(std::log2(cellsPerDimTarget)));
		//if (cellsPerDimPowerOf2 > 8) {
		//	grid.cellsPerDimMain = 8;
		//	grid.cellsPerDimSub = std::min(cellsPerDimPowerOf2 / 8, 4u);// we never go over 8 * 4 cells to save CPU
		//}
		//else {
		//	grid.cellsPerDimMain = cellsPerDimPowerOf2;
		//	grid.cellsPerDimSub = 1;
		//}
		//grid.cellsPerDim = grid.cellsPerDimMain * grid.cellsPerDimSub;

		grid.cellCountXY = std::min(cellsPerDimPowerOf2, 8u * 4u);// we never go over 8 * 4 cells to save CPU
		grid.cells.resize(grid.cellCountXY * grid.cellCountXY);

		if (grid.cellCountXY > 8) {
			grid.groupSize = grid.cellCountXY / 8;// do we actually prefer 8 * 2 over 4 * 4? 
			uint8_t layerCellsPerDim = grid.cellCountXY / grid.groupSize;
			grid.layerCells.resize(layerCellsPerDim * layerCellsPerDim);
		}

		return grid;
	}

	uint8_t rescaleToIndex(float min, float max, float coord, uint8_t indexCount)
	{
		if (coord <= min) {
			return 0;
		}
		else if (coord >= max) {
			return indexCount - 1;
		}
		else {
			float distanceNorm = (coord - min) / (max - min);
			return (uint8_t) (distanceNorm * indexCount);
		}
	}

	GridPos getGridPosForPoint(const Grid& grid, Vec2 point)
	{
		return GridPos{
			.x = rescaleToIndex(grid.boundsMin.x, grid.boundsMax.x, point.x, grid.cellCountXY),
			.y = rescaleToIndex(grid.boundsMin.y, grid.boundsMax.y, point.y, grid.cellCountXY),
		};
	}

	uint16_t getIndex(GridPos pos)
	{
		return morton::encode2d(pos.x, pos.y);
	}

	void updateGridBounds(Grid& grid, GridPos pos, const BoundingBox& bbox)
	{
		auto& cell = grid.cells.at(getIndex(pos));
		if (cell.isInUse) {
			BoundingBox::CreateMerged(cell.bbox, cell.bbox, bbox);
		}
		else {
			cell.bbox = bbox;
			cell.isInUse = true;
		}
	}
}