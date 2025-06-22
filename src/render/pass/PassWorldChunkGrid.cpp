#include "stdafx.h"
#include "PassWorldChunkGrid.h"

#include "Util.h"
#include "Render/MeshUtil.h"
#include "render/PerfStats.h"
#include "render/Gui.h"

#include <imgui.h>

namespace render::pass::world::chunkgrid
{
	using namespace DirectX;
	using std::vector;
	using ::render::grid::Grid;


	bool isInitialized = false;
	Grid grid;
	uint16_t baseCellsInUse = 0;

	grid::LayeredCells<CellCameraInfo, LayerCellCameraInfo> cellsCamera;

	struct Stats {
		uint32_t intersects = 0;
		stats::SamplerId intersectsSampler;
		stats::TimeSampler intersectsTime;
	} stats;


	uint16_t init(const Grid& gridParam)
	{
		stats.intersectsSampler = stats::createSampler();
		stats.intersectsTime = stats::createTimeSampler();

		render::gui::addInfo("World Grid", {
			[&]() -> void {
				std::stringstream buffer;
				uint32_t layerPerDim = grid.groupSize > 0 ? (grid.cellCountXY / grid.groupSizeXY) : 0;
				auto layerPerDimStr = std::to_string(layerPerDim);
				auto groupPerDimStr = std::to_string(grid.groupSizeXY);
				buffer << "Size  Outer/Inner: "
					<< layerPerDimStr << 'x' << layerPerDimStr << '/'
					<< groupPerDimStr << 'x' << groupPerDimStr << '\n';

				buffer << "Cells Total/Active: " << std::to_string(grid.cellCount) << '/' << std::to_string(baseCellsInUse) << '\n';

				uint32_t intersects = stats::getSamplerStats(stats.intersectsSampler).average;
				uint32_t intersectsTime = render::stats::getTimeSamplerStats(stats.intersectsTime).average;
				buffer << "Intersects: " << std::to_string(intersects) << " (" << std::to_string(intersectsTime) << "us)\n";

				ImGui::Text(buffer.str().c_str());
			}
		});

		grid = gridParam;
		grid::propagateBoundsToLayer(grid);
		cellsCamera.resize(grid);
		for (uint32_t i = 0; i < grid.cellCount; i++) {
			if (grid.cells.base[i].isInUse) baseCellsInUse++;
		}
		isInitialized = true;
		return grid.cellCount;
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
				std::string cellValue = cellsCamera.base[index].intersectsFrustum ? "X" : "O";
				//std::string cellValue = toStr(chunks[index].second.Extents);
				strplace(buffer, (x * cellSize) + cellSize, ::util::leftPad(cellValue, cellSize));
			}
			LOG(DEBUG) << buffer; 
		}
	}

	void updateInsersectForBaseCells(uint32_t start, uint32_t count, bool intersectsCameraFrustum)
	{
		for (uint32_t i = start; i < (start + count); i++) {
			auto& baseCell = grid.cells.base[i];
			cellsCamera.base[i].intersectsFrustum = baseCell.isInUse && intersectsCameraFrustum;
		}
	}

	void updateInsersectForBaseCells(uint32_t start, uint32_t count, const BoundingFrustum& cameraFrustum)
	{
		for (uint32_t i = start; i < (start + count); i++) {
			auto& baseCell = grid.cells.base[i];
			cellsCamera.base[i].intersectsFrustum = baseCell.isInUse && cameraFrustum.Intersects(baseCell.bbox);
			if (baseCell.isInUse) {
				stats.intersects++;
			}
		}
	}

	void updateCamera(const BoundingFrustum& cameraFrustum, bool updateCulling, bool updateDistances)
	{
		// frustum
		stats.intersects = 0;
		stats.intersectsTime.start();

		if (updateCulling) {
			if (grid.groupSize > 0) {
				for (uint32_t layerIndex = 0, baseIndex = 0; layerIndex < grid.cells.layer.size(); layerIndex++, baseIndex += grid.groupSize) {
					auto& layerCell = grid.cells.layer[layerIndex];
					auto& layerCellCamera = cellsCamera.layer[layerIndex];

					if (layerCell.isInUse) {
						ContainmentType containment = cameraFrustum.Contains(layerCell.bbox);
						layerCellCamera.intersectType = containment;
						stats.intersects++;

						uint32_t groupStart = layerIndex * grid.groupSize;

						if (containment == ContainmentType::DISJOINT) {
							updateInsersectForBaseCells(groupStart, grid.groupSize, false);
						}
						else if (containment == ContainmentType::CONTAINS) {
							updateInsersectForBaseCells(groupStart, grid.groupSize, true);
						}
						else if (containment == ContainmentType::INTERSECTS) {
							updateInsersectForBaseCells(groupStart, grid.groupSize, cameraFrustum);
						}
						else {
							assert(false);
						}
					}
				}
			}
			else {
				updateInsersectForBaseCells(0, grid.cellCount, cameraFrustum);
			}
		}

		stats.intersectsTime.sample();
		stats::takeSample(stats.intersectsSampler, stats.intersects);

		// distance
		Vec3 cameraOrigin = toVec3(cameraFrustum.Origin);
		// for chunk-based LOD, we ignore y since grid is 2D and also there are some misplaced benches deep underground in G1 (3500m under burg that mess up chunk center
		Vec2 cameraOrigin2d = { cameraFrustum.Origin.x, cameraFrustum.Origin.z };

		for (uint32_t i = 0; i < grid.cellCount; i++) {
			auto& cell = grid.cells.base[i];
			if (cell.isInUse) {
				auto& cellCamera = cellsCamera.base[i];

				// 2D (center)
				Vec2 bboxCenter2d = { cell.bbox.Center.x, cell.bbox.Center.z };
				Vec2 toBboxCenter2d = sub(cameraOrigin2d, bboxCenter2d);
				cellCamera.distanceCenter2dSq = lengthSq(toBboxCenter2d);

				// 3D (near/far corners)
				Vec3 bboxCenter = toVec3(cell.bbox.Center);
				Vec3 bboxExtents = toVec3(cell.bbox.Extents);
				Vec3 toBboxCenter = sub(cameraOrigin, bboxCenter);

				Vec3 bboxExtentsDirected = {
					std::copysign(bboxExtents.x, toBboxCenter.x),
					std::copysign(bboxExtents.y, toBboxCenter.y),
					std::copysign(bboxExtents.z, toBboxCenter.z)
				};
				Vec3 tobboxFarCorner = add(toBboxCenter, bboxExtentsDirected);
				Vec3 tobboxNearCorner = add(toBboxCenter, mul(bboxExtentsDirected, -1));

				cellCamera.distanceCornerNearSq = lengthSq(tobboxNearCorner);
				cellCamera.distanceCornerFarSq = lengthSq(tobboxFarCorner);

				assert(cellCamera.distanceCornerFarSq >= cellCamera.distanceCornerNearSq);
			}
		}
	}

	GridIndex getIndex(const GridPos& index)
	{
		uint16_t innerIndex = grid::getIndex(index);
		return {
			.outerIndex = (uint16_t)(grid.groupSize > 0 ? (innerIndex / grid.groupSize) : 0),
			.innerIndex = innerIndex
		};
	}

	LayerCellCameraInfo getCameraInfoOuter(uint16_t outerIndex)
	{
		assert(isInitialized);
		return cellsCamera.layer[outerIndex];
	}

	CellCameraInfo getCameraInfoInner(uint16_t innerIndex)
	{
		assert(isInitialized);
		return cellsCamera.base[innerIndex];
	}

	grid::CellInfo getCellInfoInner(uint16_t innerIndex)
	{
		assert(isInitialized);
		return grid.cells.base[innerIndex];
	}
}