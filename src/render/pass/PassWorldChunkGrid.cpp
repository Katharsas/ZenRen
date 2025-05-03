#include "stdafx.h"
#include "PassWorldChunkGrid.h"

#include "Util.h"

namespace render::pass::world::chunkgrid
{
	using namespace DirectX;

	int16_t minX = SHRT_MAX, minY = SHRT_MAX;
	int16_t maxX = SHRT_MIN, maxY = SHRT_MIN;
	int16_t lengthX, lengthY;
	bool sizeFinalized = false;

	std::vector<std::pair<bool, DirectX::BoundingBox>> chunks;
	std::vector<bool> chunksIntersectCamera;

	uint32_t getFlatIndex(const ChunkIndex& index)
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
			for (const auto& [chunkIndex, verts] : chunkToVerts) {
				minX = std::min(minX, chunkIndex.x);
				minY = std::min(minY, chunkIndex.y);
				maxX = std::max(maxX, chunkIndex.x);
				maxY = std::max(maxY, chunkIndex.y);
			}
		}
		lengthX = (maxX - minX) + 1;
		lengthY = (maxY - minY) + 1;
	}
	template void updateSize(const MatToChunksToVerts<VertexBasic>&);

	uint32_t finalizeSize()
	{
		assert(!sizeFinalized);

		uint32_t size = lengthX * lengthY;
		chunks.resize(size);
		chunksIntersectCamera.resize(size);
		sizeFinalized = true;
		return size;
	}

	template <VERTEX_FEATURE F>
	void updateMesh(const MatToChunksToVerts<F>& meshData)
	{
		assert(sizeFinalized);

		for (const auto& [material, chunkToVerts] : meshData) {
			for (const auto& [chunkIndex, verts] : chunkToVerts) {
				if (verts.vecPos.empty()) {
					continue;
				}
				const BoundingBox vertBbox = createBbox(verts.vecPos);
				uint32_t flatIndex = getFlatIndex(chunkIndex);
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
				std::string cellValue = chunksIntersectCamera[index] ? "X" : "O";
				//std::string cellValue = toStr(chunks[index].second.Extents);
				strplace(buffer, (x * cellSize) + cellSize, ::util::leftPad(cellValue, cellSize));
			}
			LOG(DEBUG) << buffer;
		}
	}

	void updateCamera(const BoundingFrustum& cameraFrustum)
	{
		for (uint32_t i = 0; i < chunks.size(); i++) {
			auto& [exists, existingBbox] = chunks[i];
			chunksIntersectCamera[i] = exists && cameraFrustum.Intersects(existingBbox);
		}
		// TODO remove when all works
		//printGrid();
	}

	bool intersectsCamera(const ChunkIndex& index)
	{
		assert(sizeFinalized);

		return chunksIntersectCamera[getFlatIndex(index)];
	}

	std::pair<ChunkIndex, ChunkIndex> getIndexMinMax()
	{
		return { { minX, minY }, {maxX, maxY} };
	}
}