#include "stdafx.h"
#include "GeometryBuffer.h"

namespace render::d3d
{
	void createGeometryBufUnsafe(D3d d3d, ID3D11Buffer** target, const void* data, uint32_t byteSize, BufferUsage usage, bool isIndexBuffer)
	{
		release(*target);
		D3D11_BUFFER_DESC bufferDesc = {};

		bufferDesc.Usage = (D3D11_USAGE)usage;
		bufferDesc.BindFlags = isIndexBuffer ? D3D11_BIND_INDEX_BUFFER : D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.ByteWidth = byteSize;

		D3D11_SUBRESOURCE_DATA initialData;
		initialData.pSysMem = data;
		d3d.device->CreateBuffer(&bufferDesc, &initialData, target);
	}

	void setIndexBuffer(D3d d3d, const VertexBuffer& buffer)
	{
		// TODO this sucks as well, we should not use VertexBuffer her, create raw or IndexBuffer struct and hardcode the format
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		if (buffer.stride == 2) {
			format = DXGI_FORMAT_R16_UINT;
		}
		else if (buffer.stride == 4) {
			format = DXGI_FORMAT_R32_UINT;
		}
		else {
			assert(false);
		}
		d3d.deviceContext->IASetIndexBuffer(buffer.buffer, format, 0);
	}
}
