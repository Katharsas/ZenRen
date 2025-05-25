#include "stdafx.h"
#include "PassWorldChunkGrid.h"

#include "Util.h"

namespace render::pass::world::chunkgrid
{
	using namespace DirectX;

	uint8_t minX = UCHAR_MAX, minY = UCHAR_MAX;
	uint8_t maxX = 0, maxY = 0;
	int16_t lengthX, lengthY;
	bool sizeFinalized = false;

	std::vector<std::pair<bool, DirectX::BoundingBox>> chunks;
	std::vector<ChunkCameraInfo> chunksCameraInfo;

	uint32_t getFlatIndex(const GridPos& index)
	{
		uint16_t x = index.x - minX;
		uint16_t y = index.y - minY;
		return (y * (uint32_t)lengthX) + x;
	}

	DirectX::BoundingBox createBbox(const std::vector<VertexPos> vecPos)
	{
		BoundingBox result;
		BoundingBox::CreateFromPoints(result, vecPos.size(), (const XMFLOAT3*)vecPos.data(), sizeof(VertexPos));
		return result;
	}

	template <VERTEX_FEATURE F>
	void updateSize(const MatToChunksToVerts<F>& meshData)
	{
		assert(!sizeFinalized);

		for (const auto& [material, chunkToVerts] : meshData) {
			for (const auto& [gridPos, verts] : chunkToVerts) {
				minX = std::min(minX, gridPos.x);
				minY = std::min(minY, gridPos.y);
				maxX = std::max(maxX, gridPos.x);
				maxY = std::max(maxY, gridPos.y);
			}
		}
		lengthX = (maxX - minX) + 1;
		lengthY = (maxY - minY) + 1;
	}
	template void updateSize(const MatToChunksToVerts<VertexBasic>&);

	std::pair<ChunkIndex, ChunkIndex> getIndexMinMax()
	{
		return { { minX, minY }, {maxX, maxY} };
	}

	uint32_t finalizeSize()
	{
		assert(!sizeFinalized);

		uint32_t size = lengthX * lengthY;
		chunks.resize(size);
		chunksCameraInfo.resize(size);
		sizeFinalized = true;
		return size;
	}

	template <VERTEX_FEATURE F>
	void updateMesh(const MatToChunksToVerts<F>& meshData)
	{
		assert(sizeFinalized);

		for (const auto& [material, chunkToVerts] : meshData) {
			for (const auto& [gridPos, verts] : chunkToVerts) {
				if (verts.vecPos.empty()) {
					continue;
				}
				const BoundingBox vertBbox = createBbox(verts.vecPos);
				uint32_t flatIndex = getFlatIndex(gridPos);
				auto& [exists, existingBbox] = chunks[flatIndex];
				if (exists) {
					BoundingBox::CreateMerged(existingBbox, existingBbox, vertBbox);
				}
				else {
					existingBbox = vertBbox;
					exists = true;
				}
			}
		}
	}
	template void updateMesh(const MatToChunksToVerts<VertexBasic>&);

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
		std::string buffer((lengthX * cellSize) + cellSize, ' ');

		for (uint16_t x = 0; x < lengthX; x++) {
			strplace(buffer, (x * cellSize) + cellSize, ::util::leftPad(std::to_string(minX + x), cellSize));
		}
		LOG(DEBUG) << buffer;

		for (uint16_t y = 0; y < lengthY; y++) {
			strplace(buffer, 0, ::util::leftPad(std::to_string(minY + y), cellSize));
			for (uint16_t x = 0; x < lengthX; x++) {
				uint32_t index = (y * (uint32_t)lengthX) + x;
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

		for (uint32_t i = 0; i < chunks.size(); i++) {
			// frustum
			auto& [exists, existingBbox] = chunks[i];
			chunksCameraInfo[i].intersectsFrustum = exists && cameraFrustum.Intersects(existingBbox);

			// distance
			Vec2 bboxCenter = { existingBbox.Center.x, existingBbox.Center.z };
			float distSq = lengthSq(sub(cameraOrigin, bboxCenter));
			chunksCameraInfo[i].distanceToCenterSq = distSq;
		}
	}

	ChunkCameraInfo getCameraInfo(const GridPos& index)
	{
		assert(sizeFinalized);
		return chunksCameraInfo[getFlatIndex(index)];
	}
}