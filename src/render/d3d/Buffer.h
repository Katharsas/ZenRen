#pragma once

#include "render/WinDx.h"

namespace render::d3d
{
	template<typename T>
	void createVertexBuf(D3d d3d, ID3D11Buffer** target, const std::vector<T>& vertexData, D3D11_USAGE usage = BUFFER_IMMUTABLE)
	{
		release(*target);
		D3D11_BUFFER_DESC bufferDesc = {};

		bufferDesc.Usage = usage;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.ByteWidth = sizeof(T) * vertexData.size();

		D3D11_SUBRESOURCE_DATA initialData;
		initialData.pSysMem = vertexData.data();
		d3d.device->CreateBuffer(&bufferDesc, &initialData, target);
	}

	template<typename T>
	void createVertexBuf(D3d d3d, VertexBuffer& target, const std::vector<T>& vertexData, D3D11_USAGE usage = BUFFER_IMMUTABLE)
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

	// TODO make sure usage is sensibly specified by all callers
	template<typename T>
	void createConstantBuf(D3d d3d, ID3D11Buffer** target, D3D11_USAGE usage)
	{
		release(*target);
		D3D11_BUFFER_DESC bufferDesc = {};

		bufferDesc.Usage = usage;
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.ByteWidth = sizeof(T);

		d3d.device->CreateBuffer(&bufferDesc, nullptr, target);
	}
}