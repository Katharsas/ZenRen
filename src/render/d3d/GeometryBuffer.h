#pragma once

// TODO think about maybe getting rid of windows headers here (like in ConstantBuffer.h?), but maybe code that creates geo buffers will use win.h anyway?
//      -> this would allow moving struct VertexBuffer here from Buffer.h
#include "render/WinDx.h"
#include "render/d3d/Buffer.h"

namespace render::d3d
{
	template<typename T>
	void createVertexBuf(D3d d3d, ID3D11Buffer** target, const std::vector<T>& vertexData, BufferUsage usage = BufferUsage::IMMUTABLE)
	{
		release(*target);
		D3D11_BUFFER_DESC bufferDesc = {};

		bufferDesc.Usage = (D3D11_USAGE)usage;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.ByteWidth = sizeof(T) * vertexData.size();

		D3D11_SUBRESOURCE_DATA initialData;
		initialData.pSysMem = vertexData.data();
		d3d.device->CreateBuffer(&bufferDesc, &initialData, target);
	}

	template<typename T>
	void createVertexBuf(D3d d3d, VertexBuffer& target, const std::vector<T>& vertexData, BufferUsage usage = BufferUsage::IMMUTABLE)
	{
		createVertexBuf(d3d, &target.buffer, vertexData, usage);
	}

	template<int N>
	void setVertexBuffers(D3d d3d, const std::array<VertexBuffer, N>& vertexBuffers)
	{
		std::array<UINT, N> strides;
		std::array<UINT, N> offsets;
		std::array<ID3D11Buffer*, N> buffers;

		for (uint32_t i = 0; i < N; i++) {
			const auto& buffer = vertexBuffers[i];
			strides[i] = buffer.stride;
			offsets[i] = 0;
			buffers[i] = buffer.buffer;
		}

		d3d.deviceContext->IASetVertexBuffers(0, N, buffers.data(), strides.data(), offsets.data());
	}
}