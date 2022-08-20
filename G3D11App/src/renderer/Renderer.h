#pragma once

#include <d3d11.h>

namespace renderer
{
	struct POS {
		POS(FLOAT x, FLOAT y, FLOAT z) { X = x; Y = y; Z = z; }
		FLOAT X, Y, Z;
	};
	struct UV {
		UV(FLOAT u, FLOAT v) { U = u; V = v; }
		FLOAT U, V;
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

	struct D3d {
		ID3D11Device* device;
		ID3D11DeviceContext* deviceContext;
	};

	void initD3D(HWND hWnd);
	// Clean up DirectX and COM
	void cleanD3D();
	void update();
	void renderFrame(void);
}