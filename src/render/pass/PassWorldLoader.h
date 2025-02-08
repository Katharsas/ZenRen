#pragma once

#include "../WinDx.h"
#include "../Renderer.h"

namespace render::pass::world
{
	struct World {
		bool isOutdoorLevel = true;
		std::vector<PrepassMeshes> prepassMeshes;
		std::vector<MeshBatch<VertexBasic>> meshBatchesWorld;
		std::vector<MeshBatch<VertexBlend>> meshBatchesWorldBlend;
		std::vector<MeshBatch<VertexBasic>> meshBatchesObjects;

		ID3D11ShaderResourceView* lightmapTexArray = nullptr;

		std::vector<Texture*> debugTextures;
	};

	void clearZenLevel();
	LoadWorldResult loadZenLevel(D3d d3d, const std::string& level);
}