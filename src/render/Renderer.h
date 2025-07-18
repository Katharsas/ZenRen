#pragma once

#include "Dx.h"
#include "Common.h"
#include "render/d3d/Buffer.h"
#include "Texture.h"

namespace render
{
	struct DecalMesh
	{
		ID3D11Buffer* vertexBuffer = nullptr;
		int32_t vertexCount;

		void release()
		{
			render::release(vertexBuffer);
		}
	};

	struct Mesh
	{
		uint32_t vertexCount = 0;
		VertexBuffer vbPos = { sizeof(VertexPos) };
		VertexBuffer vbOther = { sizeof(VertexBasic) };

		// Ideally, we could not create one mesh per texture, but instead have a single mesh for whole world.
		// This would require us to map every texture used for the mesh to an offset into the vertex buffer.
		// Then we could make one draw per offset and switch texture in between.
		Texture* baseColor = nullptr;

		void release()
		{
			render::release(vbPos.buffer);
			render::release(vbOther.buffer);
		}
	};

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
	};

	struct PrepassMeshes
	{
		std::vector<ChunkVertCluster> vertClusters;
		uint32_t vertexCount = 0;

		bool useIndices = false;
		VertexBuffer vbIndices = { sizeof(VertexIndex) };// meh

		VertexBuffer vbPos = { sizeof(VertexPos) };

		void release()
		{
			render::release(vbIndices.buffer);
			render::release(vbPos.buffer);
		}
	};

	struct LoadWorldResult {
		bool loaded = false;
		bool isG2 = false;
		bool isOutdoorLevel = false;
	};

	void initD3D(void* hWnd, const BufferSize& changedSize);
	bool loadLevel(const std::optional<std::string>& level, bool defaultSky = true);
	void onWindowResize(const BufferSize& changedSize);
	void onWindowDpiChange(float dpiScale);
	// Clean up DirectX and COM
	void cleanD3D();
	void update(float deltaTime);
	void renderFrame();
	void presentFrameBlocking();
}