#pragma once


#include "dx11.h"
#include "Common.h"
#include "Texture.h"

namespace render
{
	struct StaticInstance {
		std::string meshName;
		DirectX::XMMATRIX transform;
		std::array<VEC3, 2> bbox;// pos_min, pos_max
		bool receiveLightSun;
		D3DXCOLOR colLightStatic;
		DirectX::XMVECTOR dirLightStatic;// pre-inverted
	};

	struct Light {
		// TODO falloff and type surely play a role
		VEC3 pos;
		bool isStatic;
		D3DXCOLOR color;
		float range;
	};

	struct InMemoryTexFile {
		int32_t width;
		int32_t height;
		std::vector<uint8_t> ddsRaw;
	};

	struct RenderData {
		bool isOutdoorLevel;
		VERTEX_DATA_BY_MAT worldMesh;
		VERTEX_DATA_BY_MAT staticMeshes;
		std::vector<InMemoryTexFile> worldMeshLightmaps;
	};

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
		ID3D11Buffer* vertexBufferPos = nullptr;
		ID3D11Buffer* vertexBufferOther = nullptr;
		int32_t vertexCount = 0;

		// Ideally, we could not create one mesh per texture, but instead have a single mesh for whole world.
		// This would require us to map every texture used for the mesh to an offset into the vertex buffer.
		// Then we could make one draw per offset and switch texture in between.
		Texture* baseColor = nullptr;

		void release()
		{
			render::release(vertexBufferPos);
			render::release(vertexBufferOther);
		}
	};

	struct MeshBatch {
		// we could wrap this in a normal Texture object to make it more similar to unbatched meshes
		ID3D11ShaderResourceView* texColorArray;

		int32_t vertexCount = 0;
		ID3D11Buffer* vertexBufferPos = nullptr;
		ID3D11Buffer* vertexBufferOther = nullptr;
		ID3D11Buffer* vertexBufferTexIndices = nullptr;// indices into texture array (base color textures)

		void release()
		{
			render::release(texColorArray);
			render::release(vertexBufferPos);
			render::release(vertexBufferOther);
			render::release(vertexBufferTexIndices);
		}
	};

	struct PrepassMeshes
	{
		ID3D11Buffer* vertexBufferPos = nullptr;
		int32_t vertexCount = 0;

		void release()
		{
			render::release(vertexBufferPos);
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