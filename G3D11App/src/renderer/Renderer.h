#pragma once

#include "dx11.h"

namespace renderer
{
	struct BufferSize {
		uint32_t width;
		uint32_t height;
	};

	struct POS {
		POS(FLOAT x, FLOAT y, FLOAT z) { X = x; Y = y; Z = z; }
		FLOAT X, Y, Z;
	};
	struct UV {
		UV(FLOAT u, FLOAT v) { U = u; V = v; }
		FLOAT U, V;
	};

	struct POS_COLOR {
		POS position;
		D3DXCOLOR color;
	};
	struct POS_UV {
		POS position;
		UV color;
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