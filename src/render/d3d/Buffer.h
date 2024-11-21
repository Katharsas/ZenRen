#pragma once

//#include "render/Dx.h"
#include "render/WinDx.h" // TODO remove

namespace render::d3d
{
	// TODO move vertex/index buffer to GeometryBuffer file
	// TODO move constatn buffer to ConstantBuffer file
	// TODO move BufferUsage here and remove any template implementations

	template<typename T>
	void createVertexBuf(D3d d3d, ID3D11Buffer** target, const std::vector<T>& vertexData, BufferUsage usage = BufferUsage::IMMUTABLE)
	{
		release(*target);
		D3D11_BUFFER_DESC bufferDesc = {};

		bufferDesc.Usage = (D3D11_USAGE) usage;
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

	// TODO make sure usage is sensibly specified by all callers
	template<typename T>
	void createConstantBuf(D3d d3d, ID3D11Buffer** target, BufferUsage usage)
	{
		release(*target);
		D3D11_BUFFER_DESC bufferDesc = {};

		bufferDesc.Usage = (D3D11_USAGE) usage;
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.ByteWidth = sizeof(T);

		d3d.device->CreateBuffer(&bufferDesc, nullptr, target);
	}

	template<typename T>
	void updateConstantBuf(D3d d3d, ID3D11Buffer* buffer, const T& alignedData)
	{
		D3D11_BUFFER_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		BufferUsage usage = (BufferUsage)bufferDesc.Usage;
		assert(usage != BufferUsage::IMMUTABLE);

		if (usage == BufferUsage::WRITE_GPU)
		{
			d3d.deviceContext->UpdateSubresource(buffer, 0, nullptr, &alignedData, 0, 0);
			// TODO UpdateSubresource for GPU write buffers
		}
		else {
			
		}
		
		// TODO map/unmap for CPU write buffers
	}
}