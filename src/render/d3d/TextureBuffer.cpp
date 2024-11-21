#include "stdafx.h"
#include "TextureBuffer.h"

#include "render/WinDx.h";
#include "render/Primitives.h"

namespace render::d3d
{
	BufferSize getTexture2dSize(ID3D11Texture2D* buffer)
	{
		D3D11_TEXTURE2D_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);
		return {
			(uint16_t)bufferDesc.Width,
			(uint16_t)bufferDesc.Height
		};
	}

	void createTexture2dBuf(
		D3d d3d, ID3D11Texture2D** target, BufferSize size, DXGI_FORMAT format, uint16_t sampleCount, BufferUsage usage, bool writeUnordered)
	{
		release(*target);
		D3D11_TEXTURE2D_DESC desc = {};

		desc.Width = size.width;
		desc.Height = size.height;
		desc.ArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = (D3D11_USAGE) usage;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if (writeUnordered) {
			assert(usage == BufferUsage::WRITE_GPU);
			desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}

		d3d.device->CreateTexture2D(&desc, nullptr, target);
	}

	void createTexture2dBuf(
		D3d d3d, ID3D11Texture2D** target, BufferSize size, DXGI_FORMAT format, BufferUsage usage, bool writeUnordered)
	{
		createTexture2dBuf(d3d, target, size, format, 1, usage, writeUnordered);
	}

	void createTexture2dSrv(D3d d3d, ID3D11ShaderResourceView** targetSrv, ID3D11Texture2D* buffer)
	{
		release(*targetSrv);
		D3D11_TEXTURE2D_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = bufferDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = -1;

		d3d.device->CreateShaderResourceView(buffer, &srvDesc, targetSrv);
	}

	void createTexture2dUav(D3d d3d, ID3D11UnorderedAccessView** targetUav, ID3D11Texture2D* buffer)
	{
		release(*targetUav);
		D3D11_TEXTURE2D_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = bufferDesc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		d3d.device->CreateUnorderedAccessView(buffer, &uavDesc, targetUav);
	}

	void createTexture2dBufForReadbackCopy(D3d d3d, ID3D11Texture2D** target, ID3D11Texture2D* originalBuffer)
	{
		release(*target);
		D3D11_TEXTURE2D_DESC bufferDesc;
		originalBuffer->GetDesc(&bufferDesc);

		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.MiscFlags = 0;

		d3d.device->CreateTexture2D(&bufferDesc, nullptr, target);
	}

	template<typename T>
	void readbackTexture2d(D3d d3d, ID3D11Texture2D* buffer, std::function<bool(T* row, uint16_t rowIndex, uint16_t count)> processPixelRow)
	{
		D3D11_TEXTURE2D_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		BufferUsage usage = (BufferUsage)bufferDesc.Usage;
		assert(usage != BufferUsage::IMMUTABLE && usage != BufferUsage::WRITE_GPU);

		D3D11_MAPPED_SUBRESOURCE mappedResource = {};
		d3d.deviceContext->Map(buffer, 0, D3D11_MAP_READ, 0, &mappedResource);

		auto dataStart = (std::byte*)mappedResource.pData;
		for (uint32_t y = 0; y < bufferDesc.Height; y++) {
			auto rowStartBytes = dataStart + (y * mappedResource.RowPitch);
			bool processNextRow = processPixelRow((T*)rowStartBytes, y, bufferDesc.Width);
			if (!processNextRow) break;
		}

		d3d.deviceContext->Unmap(buffer, 0);
	}

	template void readbackTexture2d<float>(D3d d3d, ID3D11Texture2D* buffer, std::function<bool(float* row, uint16_t rowIndex, uint16_t count)> processPixelRow);
	template void readbackTexture2d<COLOR>(D3d d3d, ID3D11Texture2D* buffer, std::function<bool(COLOR* row, uint16_t rowIndex, uint16_t count)> processPixelRow);
}