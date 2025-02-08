#include "stdafx.h"
#include "Buffer.h"

#include "render/WinDx.h"

namespace render::d3d
{
	void createConstantBufUnsafe(D3d d3d, ID3D11Buffer** target, uint32_t byteSize, BufferUsage usage)
	{
		release(*target);
		D3D11_BUFFER_DESC bufferDesc = {};

		bufferDesc.Usage = (D3D11_USAGE)usage;
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.ByteWidth = byteSize;

		d3d.device->CreateBuffer(&bufferDesc, nullptr, target);
	}

	void updateConstantBufUnsafe(D3d d3d, ID3D11Buffer* buffer, BufferUsage usage, const void* alignedData)
	{
		assert(usage != BufferUsage::IMMUTABLE);
		if (usage == BufferUsage::WRITE_CPU) {
			// TODO map/unmap for CPU write buffers
			assert(false);
		}
		else {
			d3d.deviceContext->UpdateSubresource(buffer, 0, nullptr, alignedData, 0, 0);
		}
	}
}