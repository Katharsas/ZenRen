#include "stdafx.h"
#include "Common.h"

#include <algorithm>
#include <functional>

namespace render {

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

		viewport->TopLeftX = 0.f;
		viewport->TopLeftY = 0.f;
		viewport->Width = (float) size.width;
		viewport->Height = (float) size.height;
		viewport->MinDepth = 0.f;
		viewport->MaxDepth = 1.f;
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

	template<typename T>
	float* begin(T& t) {
		return t.vec;
	}
	template<typename T>
	const float* begin(const T& t) {
		return t.vec;
	}
	template<typename T>
	const float* end(const T& t) {
		return t.vec + std::size(t.vec);
	}

	template<typename T>
	T add(const T& vec1, const T& vec2) {
		T result;
		std::transform(begin(vec1), end(vec1), begin(vec2), begin(result), std::plus<float>());
		return result;
	}
	template<typename T>
	T mul(const T& vec, float scalar) {
		T result;
		std::transform(begin(vec), end(vec), begin(result), std::bind(std::multiplies<float>(), std::placeholders::_1, scalar));
		return result;
	}

	// instantiate template functions for the types we want to support (needed because templates are not in header)
	template UV add(const UV& vec1, const UV& vec2);
	template UV mul(const UV& vec, float scalar);
}