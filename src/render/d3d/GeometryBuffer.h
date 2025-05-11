#pragma once

// TODO think about maybe getting rid of windows headers here (like in ConstantBuffer.h?), but maybe code that creates geo buffers will use win.h anyway?
//      -> this would allow moving struct VertexBuffer here from Buffer.h
#include "render/WinDx.h"
#include "render/d3d/Buffer.h"

namespace render::d3d
{
	void createGeometryBufUnsafe(D3d d3d, ID3D11Buffer** target, const void* data, uint32_t byteSize, BufferUsage usage, bool isIndexBuffer);

	// TODO combine these? use GeoBufferType flag?
	template<typename T>
	void createVertexBuf(D3d d3d, ID3D11Buffer** target, const std::vector<T>& vertexData, BufferUsage usage = BufferUsage::IMMUTABLE)
	{
		uint32_t byteSize = sizeof(T) * vertexData.size();
		createGeometryBufUnsafe(d3d, target, vertexData.data(), byteSize, usage, false);
	}

	template<typename T>
	void createIndexBuf(D3d d3d, ID3D11Buffer** target, const std::vector<T>& indexData, BufferUsage usage = BufferUsage::IMMUTABLE)
	{
		uint32_t byteSize = sizeof(T) * indexData.size();
		createGeometryBufUnsafe(d3d, target, indexData.data(), byteSize, usage, true);
	}

	// TODO remove these? we don't even need the additional info in VertexBuffer? move it into VertexBuffer?

	template<typename T>
	void createVertexBuf(D3d d3d, VertexBuffer& target, const std::vector<T>& vertexData, BufferUsage usage = BufferUsage::IMMUTABLE)
	{
		createVertexBuf(d3d, &target.buffer, vertexData, usage);
	}

	template<typename T>
	void createIndexBuf(D3d d3d, VertexBuffer& target, const std::vector<T>& vertexData, BufferUsage usage = BufferUsage::IMMUTABLE)
	{
		createIndexBuf(d3d, &target.buffer, vertexData, usage);
	}

	// TODO these set methods suck massively

	void setIndexBuffer(D3d d3d, const VertexBuffer& buffer);

	void setVertexBuffers(D3d d3d, const std::vector<VertexBuffer>& vertexBuffers);
}