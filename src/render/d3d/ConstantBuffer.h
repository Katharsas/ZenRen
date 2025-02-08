#pragma once

#include "render/Dx.h"
#include "render/d3d/Buffer.h"

namespace render::d3d
{
	void createConstantBufUnsafe(D3d d3d, ID3D11Buffer** target, uint32_t byteSize, BufferUsage usage);
	void updateConstantBufUnsafe(D3d d3d, ID3D11Buffer* buffer, BufferUsage usage, const void* alignedData);

	// TODO make sure usage is sensibly specified by all callers
	template<typename T>
	void createConstantBuf(D3d d3d, ID3D11Buffer** target, BufferUsage usage)
	{
		constexpr uint32_t byteSize = sizeof(T);
		static_assert(byteSize % 16 == 0);// constant buffer data must be 16 bit aligned
		createConstantBufUnsafe(d3d, target, byteSize, usage);
	}

	template<typename T>
	void updateConstantBuf(D3d d3d, ID3D11Buffer* buffer, const T& alignedData)
	{
		D3D11_BUFFER_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		assert(bufferDesc.ByteWidth == sizeof(T));
		updateConstantBufUnsafe(d3d, buffer, (BufferUsage)bufferDesc.Usage, &alignedData);
	}
}