#pragma once

#include "Renderer.h"

namespace render::util
{
	void dumpVerts(const std::string& matName, const std::vector<VERTEX_POS>& vertPos, const std::vector<NORMAL_UV_LUV>& vertOther);
	std::string getVramUsage(IDXGIAdapter3* adapter);

	// TODO all of the following should probably go into pipelineutil

	template<typename T>
	void createVertexBuffer(D3d d3d, ID3D11Buffer** target, const std::vector<T>& vertexData)
	{
		release(*target);

		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(bufferDesc));

		bufferDesc.Usage = D3D11_USAGE_DEFAULT;// TODO should be configurable and probably different both for static and for per-frame buffers
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.ByteWidth = sizeof(T) * vertexData.size();

		D3D11_SUBRESOURCE_DATA initialData;
		initialData.pSysMem = vertexData.data();
		d3d.device->CreateBuffer(&bufferDesc, &initialData, target);
	}

	template<typename T>
	void createVertexBuffer(D3d d3d, VertexBuffer& target, const std::vector<T>& vertexData)
	{
		createVertexBuffer(d3d, &target.buffer, vertexData);
	}

	template<typename T>
	void createConstantBuffer(D3d d3d, ID3D11Buffer** target, D3D11_USAGE usage)
	{
		release(*target);

		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));

		bufferDesc.Usage = usage;
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.ByteWidth = sizeof(T);

		d3d.device->CreateBuffer(&bufferDesc, nullptr, target);
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

