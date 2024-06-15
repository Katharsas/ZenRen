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
		Light_Sun,
		Light_Static
	};

	struct ShaderSettings {
		ShaderMode mode;
	};

	struct RenderSettings {
		bool wireframe = false;
		bool reverseZ = true;

		bool anisotropicFilter = true;
		uint32_t anisotropicLevel = 16;

		ShaderSettings shader = {
			ShaderMode::Default,
		};

		float resolutionScaling = 1.0f;
		bool resolutionUpscaleSmooth = true;
		bool downsampling = false;// does not work currently, TODO implement mipmap generation for linear BB

		uint32_t multisampleCount = 4;
		bool multisampleTransparency = true;
		bool distantAlphaDensityFix = true;
		
		bool depthPrepass = false;

		bool skyTexBlur = true;
	};

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
		std::unordered_map<Material, VEC_VERTEX_DATA> worldMesh;
		std::unordered_map<Material, VEC_VERTEX_DATA> staticMeshes;
		std::vector<InMemoryTexFile> worldMeshLightmaps;
	};

	struct DecalMesh
	{
		ID3D11Buffer* vertexBuffer = nullptr;
		int32_t vertexCount;

		void release()
		{
			renderer::release(vertexBuffer);
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
			renderer::release(vertexBufferPos);
			renderer::release(vertexBufferOther);
		}
	};

	struct PrepassMeshes
	{
		ID3D11Buffer* vertexBufferPos = nullptr;
		int32_t vertexCount = 0;

		void release()
		{
			renderer::release(vertexBufferPos);
		}
	};

	struct ShaderCbs
	{
		ID3D11Buffer* settingsCb = nullptr;
		ID3D11Buffer* cameraCb = nullptr;
	};

	void initD3D(HWND hWnd);
	void loadLevel(std::string& level);
	void onWindowResize(uint32_t width, uint32_t height);
	// Clean up DirectX and COM
	void cleanD3D();
	void update(float deltaTime);
	void renderFrame();
}