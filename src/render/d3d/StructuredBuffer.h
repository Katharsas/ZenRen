#pragma once

#include "render/Dx.h"
#include "render/basic/Graphics.h"

namespace render::d3d
{
	void createStructuredBufUnsafe(D3d d3d, ID3D11Buffer** target, const void* data, uint32_t count, uint32_t stride, BufferUsage usage, bool writeUnordered);

	template<typename T>
	void createStructuredBuf(
		D3d d3d, ID3D11Buffer** target, const std::vector<T>& data, BufferUsage usage = BufferUsage::IMMUTABLE, bool writeUnordered = false)
	{
		createStructuredBufUnsafe(d3d, target, data.data(), data.size(), sizeof(T), usage, writeUnordered);
	}

	template<typename T>
	void createStructuredBuf(
		D3d d3d, ID3D11Buffer** target, uint32_t count, BufferUsage usage, bool writeUnordered = false)
	{
		createStructuredBufUnsafe(d3d, target, nullptr, count, sizeof(T), usage, writeUnordered);
	}

	void createStructuredSrv(D3d d3d, ID3D11ShaderResourceView** targetSrv, ID3D11Buffer* buffer);
	void createStructuredUav(D3d d3d, ID3D11UnorderedAccessView** targetUav, ID3D11Buffer* buffer);
	void createStructuredBufForReadbackCopy(D3d d3d, ID3D11Buffer** target, ID3D11Buffer* originalBuffer);
}