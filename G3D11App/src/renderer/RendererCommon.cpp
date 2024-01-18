#include "stdafx.h"
#include "RendererCommon.h"

namespace renderer {

	void release(IUnknown* dx11object) {
		if (dx11object != nullptr) {
			dx11object->Release();
		}
	}

	void initViewport(BufferSize& size, D3D11_VIEWPORT* viewport) {
		ZeroMemory(viewport, sizeof(D3D11_VIEWPORT));

		viewport->TopLeftX = 0;
		viewport->TopLeftY = 0;
		viewport->Width = size.width;
		viewport->Height = size.height;
		viewport->MinDepth = 0.0f;
		viewport->MaxDepth = 1.0f;
	}
}