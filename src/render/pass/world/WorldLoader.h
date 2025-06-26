#pragma once

#include "render/WinDx.h"
#include "render/Renderer.h"

namespace render::pass::world
{
	template <VERTEX_FEATURE F>
	struct MeshBatches {
		std::array<std::vector<MeshBatch<F>>, BLEND_TYPE_COUNT> passes;

		std::vector<MeshBatch<VertexBasic>>& getBatches(BlendType blendType)
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
		std::vector<PrepassMeshes> prepassMeshes;
		MeshBatches<VertexBasic> meshBatchesWorld;
		MeshBatches<VertexBasic> meshBatchesObjects;

		ID3D11ShaderResourceView* lightmapTexArray = nullptr;

		std::vector<Texture*> debugTextures;
	};

	void clearZenLevel();
	LoadWorldResult loadZenLevel(D3d d3d, const std::string& level);
}