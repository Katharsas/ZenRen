#pragma once

#include "render/Dx.h"
#include "render/Renderer.h"

namespace render::pass::world
{
	struct MeshBatches {
		std::array<std::vector<MeshBatch>, BLEND_TYPE_COUNT> passes;

		std::vector<MeshBatch>& getBatches(BlendType blendType)
		{
			return passes.at((uint8_t) blendType);
		}
		void release()
		{
			for (auto& meshes : passes) {
				for (auto& meshBatch : meshes) {
					meshBatch.release();
				}
				meshes.clear();
			}
		}
	};

	struct World {
		bool isOutdoorLevel = true;
		MeshBatches meshBatchesWorld;
		MeshBatches meshBatchesObjects;

		ID3D11ShaderResourceView* staticInstancesSb = nullptr;
		ID3D11ShaderResourceView* lightmapTexArray = nullptr;

		std::vector<Texture*> debugTextures;
	};

	void clearZenLevel();
	LoadWorldResult loadZenLevel(D3d d3d, const std::string& level);
}