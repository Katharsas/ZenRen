#include "stdafx.h"
#include "StructuredBuffer.h"

#include <dxgiformat.h>

#include "render/WinDx.h";

namespace render::d3d
{
	void createStructuredBufUnsafe(D3d d3d, ID3D11Buffer** target, const void* data, uint32_t count, uint32_t stride, BufferUsage usage, bool writeUnordered)
	{
		// TODO should writeUnordered ALWAYS be true for all BUFFER_WRITE_GPU usages?

		// https://www.gamedev.net/forums/topic/709796-working-with-structuredbuffer-in-hlsl-directx-11/
		// https://github.com/walbourn/directx-sdk-samples/blob/main/BasicCompute11/BasicCompute11.cpp#L500

		release(*target);
		D3D11_BUFFER_DESC desc = {};

		desc.Usage = (D3D11_USAGE)usage;
		if (usage == BufferUsage::READBACK) {
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		}
		else {
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		}
		if (writeUnordered) {
			assert(usage == BufferUsage::WRITE_GPU);
			desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		desc.StructureByteStride = stride;
		desc.ByteWidth = stride * count;

		if (data) {
			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = data;

			d3d.device->CreateBuffer(&desc, &initialData, target);
		}
		else {
			d3d.device->CreateBuffer(&desc, nullptr, target);
		}
	}

	void createStructuredSrv(D3d d3d, ID3D11ShaderResourceView** targetSrv, ID3D11Buffer* buffer)
	{
		release(*targetSrv);
		D3D11_BUFFER_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		assert(bufferDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		srvDesc.BufferEx.FirstElement = 0;
		srvDesc.BufferEx.NumElements = bufferDesc.ByteWidth / bufferDesc.StructureByteStride;;

		d3d.device->CreateShaderResourceView(buffer, &srvDesc, targetSrv);
	}

	void createStructuredUav(D3d d3d, ID3D11UnorderedAccessView** targetUav, ID3D11Buffer* buffer)
	{
		release(*targetUav);
		D3D11_BUFFER_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		assert(bufferDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = bufferDesc.ByteWidth / bufferDesc.StructureByteStride;

		d3d.device->CreateUnorderedAccessView(buffer, &uavDesc, targetUav);
	}

	void createStructuredBufForReadbackCopy(D3d d3d, ID3D11Buffer** target, ID3D11Buffer* originalBuffer)
	{
		release(*target);
		D3D11_BUFFER_DESC bufferDesc;
		originalBuffer->GetDesc(&bufferDesc);

		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.MiscFlags = 0;

		d3d.device->CreateBuffer(&bufferDesc, nullptr, target);
	}
}