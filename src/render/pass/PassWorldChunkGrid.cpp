#include "stdafx.h"
#include "PassWorldChunkGrid.h"

#include "Util.h"

namespace render::pass::world::chunkgrid
{
	using namespace DirectX;
	using std::vector;

	// TODO calculate empty areas?
	//uint8_t minX = UCHAR_MAX, minY = UCHAR_MAX;
	//uint8_t maxX = 0, maxY = 0;

	bool isInitialized = false;
	Grid grid;
	uint16_t size = 0;

	vector<ChunkCameraInfo> chunksCameraInfo;

	uint16_t init(const Grid& gridParam)
	{
		grid = gridParam;
		size = grid.cellCountXY * grid.cellCountXY;
		chunksCameraInfo.resize(size);

		// TODO hierarchy

		isInitialized = true;
		return size;
	}

	std::pair<GridPos, GridPos> getIndexMinMax()
	{
		return { { 0u, 0u }, { grid.cellCountXY, grid.cellCountXY } };
	}


	void strplace(std::string& str, uint32_t index, const std::string& str2)
	{
		for (uint32_t i = 0; i < str2.size(); i++) {
			str[index + i] = str2[i];
		}
	}

	std::string toStr(XMFLOAT3 xmf3)
	{
		return "(" + ::util::leftPad(std::to_string((int32_t) xmf3.x), 4) + ' ' + ::util::leftPad(std::to_string((int32_t) xmf3.z), 4) + ")";
	}

	void printGrid(uint16_t cellSize = 3)
	{
		float size = grid.cellCountXY;
		std::string buffer((size * cellSize) + cellSize, ' ');

		for (uint8_t x = 0; x < size; x++) {
			strplace(buffer, (x * cellSize) + cellSize, ::util::leftPad(std::to_string(x), cellSize));
		}
		LOG(DEBUG) << buffer;

		for (uint8_t y = 0; y < size; y++) {
			strplace(buffer, 0, ::util::leftPad(std::to_string(y), cellSize));
			for (uint8_t x = 0; x < size; x++) {
				uint32_t index = grid::getIndex({ x, y });
				std::string cellValue = chunksCameraInfo[index].intersectsFrustum ? "X" : "O";
				//std::string cellValue = toStr(chunks[index].second.Extents);
				strplace(buffer, (x * cellSize) + cellSize, ::util::leftPad(cellValue, cellSize));
			}
			LOG(DEBUG) << buffer; 
		}
	}

	void updateCamera(const BoundingFrustum& cameraFrustum)
	{
		// we ignore y since grid is 2D and also there are some misplaced benches deep underground in G1 (3500m under burg that mess up chunk center
		Vec2 cameraOrigin = { cameraFrustum.Origin.x, cameraFrustum.Origin.z };

		for (uint32_t i = 0; i < size; i++) {
			// frustum
			auto& cell = grid.cells[i];
			chunksCameraInfo[i].intersectsFrustum = cell.isInUse && cameraFrustum.Intersects(cell.bbox);

			// distance
			Vec2 bboxCenter = { cell.bbox.Center.x, cell.bbox.Center.z };
			float distSq = lengthSq(sub(cameraOrigin, bboxCenter));
			chunksCameraInfo[i].distanceToCenterSq = distSq;
		}
	}

	ChunkCameraInfo getCameraInfo(const GridPos& index)
	{
		assert(isInitialized);
		return chunksCameraInfo[grid::getIndex(index)];
	}
}