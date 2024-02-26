#pragma once


#include "dx11.h"
#include "RendererCommon.h"
#include "Texture.h"

namespace renderer
{
	enum ShaderMode {
		Default,
		Solid,
		Diffuse,
		Normals,
		Lightmap,
	};

	struct ShaderSettings {
		float ambientLight;
		ShaderMode mode;
	};

	struct RenderSettings {
		bool wireframe = false;
		bool reverseZ = true;

		bool anisotropicFilter = true;
		uint32_t anisotropicLevel = 16;

		ShaderSettings shader = {
			0.02f,
			ShaderMode::Default,
		};

		float resolutionScaling = 1.0f;
		bool resolutionUpscaleSmooth = true;
		uint32_t multisampleCount = 4;

		bool downsampling = false;// does not work currently
		
	};

	struct StaticInstance {
		std::string meshName;
		DirectX::XMMATRIX transform;
	};

	struct InMemoryTexFile {
		int32_t width;
		int32_t height;
		std::vector<uint8_t> ddsRaw;
	};

	struct RenderData {
		std::unordered_map<Material, VEC_POS_NORMAL_UV_LMUV> worldMesh;
		std::unordered_map<Material, VEC_POS_NORMAL_UV_LMUV> staticMeshes;
		std::vector<InMemoryTexFile> worldMeshLightmaps;
	};

	struct DecalMesh
	{
		ID3D11Buffer* vertexBuffer = nullptr;
		int32_t vertexCount;
		Texture* baseColor = nullptr;

		void release()
		{
			renderer::release(vertexBuffer);
			if (baseColor != nullptr) {
				delete baseColor;
			}
		}
	};

	struct Mesh
	{
		ID3D11Buffer* vertexBufferPos = nullptr;
		ID3D11Buffer* vertexBufferNormalUv = nullptr;
		int32_t vertexCount;

		// Ideally, we could not create one mesh per texture, but instead have a single mesh for whole world.
		// This would require us to map every texture used for the mesh to an offset into the vertex buffer.
		// Then we could make one draw per offset and switch texture in between.
		Texture* baseColor = nullptr;

		void release()
		{
			renderer::release(vertexBufferPos);
			renderer::release(vertexBufferNormalUv);
			if (baseColor != nullptr) {
				delete baseColor;
			}
		}
	};

	struct MeshIndexed
	{
		ID3D11Buffer* vertexBuffer = nullptr;
		ID3D11Buffer* indexBuffer = nullptr;
		int32_t indexCount;

		void release()
		{
			if (vertexBuffer != nullptr) {
				vertexBuffer->Release();
			}
			if (indexBuffer != nullptr) {
				indexBuffer->Release();
			}
		}
	};

	void initD3D(HWND hWnd);
	void loadLevel(std::string& level);
	void onWindowResize(uint32_t width, uint32_t height);
	// Clean up DirectX and COM
	void cleanD3D();
	void update();
	void renderFrame();
}