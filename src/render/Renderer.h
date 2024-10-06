#pragma once

#include "WinDx.h"
#include "Common.h"
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

	struct VertexBuffer {
		uint32_t stride = -1;
		ID3D11Buffer* buffer = nullptr;
	};

	struct Mesh
	{
		uint32_t vertexCount = 0;
		VertexBuffer vbPos = { sizeof(VERTEX_POS) };
		VertexBuffer vbOther = { sizeof(VERTEX_OTHER) };

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

	struct MeshBatch {
		// we could wrap this in a normal Texture object to make it more similar to unbatched meshes
		ID3D11ShaderResourceView* texColorArray;

		std::vector<ChunkVertCluster> vertClusters;

		uint32_t vertexCount = 0;
		VertexBuffer vbPos = { sizeof(VERTEX_POS) };
		VertexBuffer vbOther = { sizeof(VERTEX_OTHER) };
		VertexBuffer vbTexIndices = { sizeof(TEX_INDEX) };// indices into texture array (base color textures)

		void release()
		{
			render::release(texColorArray);
			render::release(vbPos.buffer);
			render::release(vbOther.buffer);
			render::release(vbTexIndices.buffer);
		}
	};

	struct PrepassMeshes
	{
		std::vector<ChunkVertCluster> vertClusters;

		uint32_t vertexCount = 0;
		VertexBuffer vbPos = { sizeof(VERTEX_POS) };

		void release()
		{
			render::release(vbPos.buffer);
		}
	};

	struct ShaderCbs
	{
		ID3D11Buffer* settingsCb = nullptr;
		ID3D11Buffer* cameraCb = nullptr;
	};

	void initD3D(HWND hWnd, const BufferSize& changedSize);
	void loadLevel(std::string& level);
	void onWindowResize(const BufferSize& changedSize);
	// Clean up DirectX and COM
	void cleanD3D();
	void update(float deltaTime);
	void renderFrame();
}