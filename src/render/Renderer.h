#pragma once

#include "Dx.h"
#include "Common.h"
#include "render/d3d/Shader.h";
#include "render/d3d/GeometryBuffer.h"
#include "Texture.h"

namespace render
{
	// TODO rename to MeshBatchBuffers?
	template <VERTEX_FEATURE F>
	struct MeshBatch {
		// we could wrap this in a normal Texture object to make it more similar to unbatched meshes
		ID3D11ShaderResourceView* texColorArray = nullptr;

		uint32_t drawCount = 0;
		uint32_t drawLodCount = 0;
		std::vector<ChunkVertCluster> vertClusters;
		std::vector<ChunkVertCluster> vertClustersLod;

		bool useIndices = false;
		VertexBuffer vbIndices = { sizeof(VertexIndex) };

		VertexBuffer vbPos = { sizeof(VertexPos) };
		VertexBuffer vbOther = { sizeof(F) };
		VertexBuffer vbTexIndices = { sizeof(TexIndex) };// indices into texture array (base color textures)

		void release()
		{
			render::release(texColorArray);
			render::release(vbIndices.buffer);
			render::release(vbPos.buffer);
			render::release(vbOther.buffer);
			render::release(vbTexIndices.buffer);
		}

		static std::vector<d3d::VertexAttributeDesc> shaderLayout()
		{
			return d3d::buildInputLayoutDesc({
				inputLayout<VertexPos>(),
				inputLayout<F>(),
				inputLayout<TexIndex>(),
			});
		}
	};

	struct LoadWorldResult {
		bool loaded = false;
		bool isG2 = false;
		bool isOutdoorLevel = false;
	};

	void initD3D(WindowHandle hWnd, const BufferSize& changedSize);
	bool loadLevel(const std::optional<std::string>& level, bool defaultSky = true);
	void onWindowResize(const BufferSize& changedSize);
	void onWindowDpiChange(float dpiScale);
	// Clean up DirectX and COM
	void cleanD3D();
	void update(float deltaTime);
	void renderFrame();
	void presentFrameBlocking();
	void reloadShaders();
}