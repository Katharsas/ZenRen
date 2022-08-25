#pragma once

#include <stdint.h>

#include "dx11.h"

namespace renderer
{
	struct RenderSettings {
		bool wireframe = false;
		bool reverseZ = true;

		bool anisotropicFilter = true;
		uint32_t anisotropicLevel = 16;
	};

	struct BufferSize {
		uint32_t width;
		uint32_t height;
	};

	struct POS {
		FLOAT x, y, z;

		friend std::ostream& operator <<(std::ostream& os, const POS& that)
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
		POS position;
		D3DXCOLOR color;
	};
	struct POS_UV {
		POS pos;
		UV uv;

		friend std::ostream& operator <<(std::ostream& os, const POS_UV& that)
		{
			return os << "[POS: " << that.pos << " UV:" << that.uv << "]";
		}
	};

	struct Mesh
	{
		ID3D11Buffer* vertexBuffer;
		int32_t vertexCount;

		void release()
		{
			vertexBuffer->Release();
		}
	};

	struct MeshIndexed
	{
		ID3D11Buffer* vertexBuffer;
		ID3D11Buffer* indexBuffer;
		int32_t indexCount;

		void release()
		{
			vertexBuffer->Release();
			indexBuffer->Release();
		}
	};

	void initD3D(HWND hWnd);
	void onWindowResize(uint32_t width, uint32_t height);
	// Clean up DirectX and COM
	void cleanD3D();
	void update();
	void renderFrame();
}