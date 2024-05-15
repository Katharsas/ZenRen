#include "stdafx.h"
#include "RendererCommon.h"

namespace renderer {

	void release(IUnknown* dx11object) {
		if (dx11object != nullptr) {
			dx11object->Release();
		}
	}

	void release(const std::vector<IUnknown*>& dx11objects) {
		for (auto object : dx11objects) {
			release(object);
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

	float fromSRGB(const float channel) {
		return (channel <= 0.04045f) ? (channel / 12.92f) : pow((channel + 0.055f) / 1.055f, 2.4f);
	}

	D3DXCOLOR fromSRGB(const D3DXCOLOR color) {
		return D3DXCOLOR(
			fromSRGB(color.r),
			fromSRGB(color.g),
			fromSRGB(color.b),
			color.a);
	}

	D3DXCOLOR greyscale(const float channel) {
		return D3DXCOLOR(channel, channel, channel, 1);
	}

	D3DXCOLOR multiplyColor(D3DXCOLOR color, const float factor) {
		return D3DXCOLOR(
			color.r * factor,
			color.g * factor,
			color.b * factor,
			color.a);
	}
}