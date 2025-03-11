#include "stdafx.h"
#include "Common.h"

namespace render {

	void forEachFace(const MatToChunksToVertsBasic& data, const std::function<void(const VertKey& vertKey)>& func)
	{
		for (const auto& [material, chunkData] : data) {
			for (const auto& [chunkIndex, vertData] : chunkData) {
				uint32_t vertCount = vertData.useIndices ? vertData.vecIndex.size() : vertData.vecPos.size();
				for (uint32_t i = 0; i < vertCount; i += 3) {
					const VertKey vertKey {
						.mat = material,
						.chunkIndex = chunkIndex,
						.vertIndex = i,
					};
					func(vertKey);
				}
			}
		}
	}
}