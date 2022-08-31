#pragma once

#include <stdint.h>

#include "dx11.h"

namespace renderer
{
	enum ShaderMode {
		Default,
		Solid,
		Diffuse,
		Normals,
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
	};

	struct BufferSize {
		uint32_t width;
		uint32_t height;
	};

	struct VEC3 {
		FLOAT x, y, z;

		friend std::ostream& operator <<(std::ostream& os, const VEC3& that)
		{
			return os << "[X=" << that.x << " Y=" << that.y << " Z=" << that.z << "]";
		}
	};
	struct UV {
		FLOAT u, v;

		friend std::ostream& operator <<(std::ostream& os, const UV& that)
		{
			return os << "[U=" << that.u << " V=" << that.v << "]";
		}
	};

	struct POS_COLOR {
		VEC3 position;
		D3DXCOLOR color;
	};
	struct POS_UV {
		VEC3 pos;
		UV uv;

		friend std::ostream& operator <<(std::ostream& os, const POS_UV& that)
		{
			return os << "[POS: " << that.pos << " UV:" << that.uv << "]";
		}
	};
	struct POS_NORMAL_UV {
		VEC3 pos;
		VEC3 normal;
		UV uv;

		friend std::ostream& operator <<(std::ostream& os, const POS_NORMAL_UV& that)
		{
			return os << "[POS:" << that.pos << " NOR:" << that.normal << " UV:" << that.uv << "]";
		}
	};

	struct Mesh
	{
		ID3D11Buffer* vertexBuffer = nullptr;
		int32_t vertexCount;

		void release()
		{
			if (vertexBuffer != nullptr) {
				vertexBuffer->Release();
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
	void onWindowResize(uint32_t width, uint32_t height);
	// Clean up DirectX and COM
	void cleanD3D();
	void update();
	void renderFrame();
}