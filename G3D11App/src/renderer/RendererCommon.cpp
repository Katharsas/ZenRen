#include "stdafx.h"
#include "RendererCommon.h"

namespace renderer {

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