#pragma once

#include "render/Dx.h"
#include "render/d3d/Buffer.h"

namespace render::d3d
{
	void createConstantBufUnsafe(D3d d3d, ID3D11Buffer** target, uint32_t byteSize, BufferUsage usage);
	void updateConstantBufUnsafe(D3d d3d, ID3D11Buffer* buffer, BufferUsage usage, const void* alignedData);

	// TODO there should be exactly 1 slot used per CB type, and this should be managed centrally (ShaderManager?)
	// TODO maybe also have struct "ConstantData" that wraps user structs and does alignment and slot stuff automatically?
	template <typename T>
	struct ConstantBuffer {
		// TODO struct should probably be non-copyable
		ID3D11Buffer* buffer = nullptr;

		void release()
		{
			render::release(buffer);
		}
	};

	// TODO make sure usage is sensibly specified by all callers
	template<typename T>
	void createConstantBuf(D3d d3d, ConstantBuffer<T>& target, BufferUsage usage)
	{
		constexpr uint32_t byteSize = sizeof(T);
		static_assert(byteSize % 16 == 0);// constant buffer data must be 16 bit aligned
		createConstantBufUnsafe(d3d, &target.buffer, byteSize, usage);
	}

	template<typename T>
	void updateConstantBuf(D3d d3d, ConstantBuffer<T> buffer, const T& alignedData)
	{
		D3D11_BUFFER_DESC bufferDesc;
		buffer.buffer->GetDesc(&bufferDesc);

		assert(bufferDesc.ByteWidth == sizeof(T));
		updateConstantBufUnsafe(d3d, buffer.buffer, (BufferUsage)bufferDesc.Usage, &alignedData);
	}
}