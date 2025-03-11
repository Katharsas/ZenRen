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
		D3d d3d, ID3D11Texture2D** target, BufferSize size, DXGI_FORMAT format, uint16_t sampleCount,
		BufferUsage usage, bool writeUnordered, std::vector<InitialData> initialMips)
	{
		release(*target);
		D3D11_TEXTURE2D_DESC desc = {};

		desc.Width = size.width;
		desc.Height = size.height;
		desc.ArraySize = 1;
		desc.MipLevels = initialMips.empty() ? 1 : initialMips.size();
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = (D3D11_USAGE) usage;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if (writeUnordered) {
			assert(usage == BufferUsage::WRITE_GPU);
			desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}

		std::vector<D3D11_SUBRESOURCE_DATA> initialData;
		for (const auto& initialMip : initialMips) {
			const auto& surface = initialMip.surface;
			initialData.push_back({
				.pSysMem = initialMip.dataPtr,
				.SysMemPitch = surface.bytesPerRow,
				.SysMemSlicePitch = surface.bytesPerRow * surface.rowCount,
			});
		}
		auto* initialDataPtr = initialData.empty() ? nullptr : initialData.data();
		auto hr = d3d.device->CreateTexture2D(&desc, initialDataPtr, target);
		::util::throwOnError(hr, "createTexture2dBuf failed!");
	}

	void createTexture2dBuf(
		D3d d3d, ID3D11Texture2D** target, BufferSize size, DXGI_FORMAT format, BufferUsage usage, bool writeUnordered)
	{
		createTexture2dBuf(d3d, target, size, format, 1, usage, writeUnordered);
	}

	void createTexture2dBuf(
		D3d d3d, ID3D11Texture2D** target, BufferSize size, DXGI_FORMAT format, std::vector<InitialData> initialMips,
		BufferUsage usage)
	{
		createTexture2dBuf(d3d, target, size, format, 1, usage, false, initialMips);
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
		srvDesc.Texture2D.MipLevels = bufferDesc.MipLevels;

		auto hr = d3d.device->CreateShaderResourceView(buffer, &srvDesc, targetSrv);
		::util::throwOnError(hr, "createTexture2dSrv failed!");
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

		auto hr = d3d.device->CreateUnorderedAccessView(buffer, &uavDesc, targetUav);
		::util::throwOnError(hr);
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

		auto hr = d3d.device->CreateTexture2D(&bufferDesc, nullptr, target);
		::util::throwOnError(hr, "createTexture2dBufForReadbackCopy failed!");
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


	void createTexture2dBuf(
		D3d d3d, ID3D11Texture2D** target, BufferSize size, DXGI_FORMAT format, uint16_t sampleCount,
		BufferUsage usage, bool writeUnordered, std::vector<InitialData> initialMips); 


	void createTexture2dArrayBufByCopy(D3d d3d, ID3D11Texture2D** target, std::vector<ID3D11Texture2D*> buffersToCopy, BufferUsage usage)
	{
		assert(usage != BufferUsage::IMMUTABLE);
		release(*target);
		// TODO maybe we should make it possible to initialize an immutable array by passing createTexture2dBuf with initialMipsAndSlices
		// and maybe createTextureFromGothicTex can be passed a lamda that decides how/if to initialize anything
		// Maybe createTextureFromGothicTex can allocate all data pointers on the heap that need to persist until texture creation
		// and put them into a cleanup vector that is also passed by the caller?

		// we assume here that all original buffers have an identical buffer description
		D3D11_TEXTURE2D_DESC bufferDesc;
		buffersToCopy.at(0)->GetDesc(&bufferDesc);
		bufferDesc.ArraySize = buffersToCopy.size();
		bufferDesc.Usage = (D3D11_USAGE) usage;
		bufferDesc.MiscFlags = bufferDesc.MiscFlags;

		auto hr = d3d.device->CreateTexture2D(&bufferDesc, nullptr, target);
		::util::throwOnError(hr, "createTexture2dArrayBuf failed!");

		uint32_t mipCount = bufferDesc.MipLevels;

		// copy all mips for each buffer
		for (uint32_t bufferIndex = 0; bufferIndex < buffersToCopy.size(); bufferIndex++) {
			for (uint32_t mipIndex = 0; mipIndex < mipCount; mipIndex++) {
				uint32_t subresourceIndex = (bufferIndex * mipCount) + mipIndex;
				d3d.deviceContext->CopySubresourceRegion(*target, subresourceIndex, 0, 0, 0, buffersToCopy.at(bufferIndex), mipIndex, nullptr);
			}
		}
	}

	void createTexture2dArraySrv(D3d d3d, ID3D11ShaderResourceView** targetSrv, ID3D11Texture2D* buffer, uint32_t reduceMipLevelBy)
	{
		release(*targetSrv);
		D3D11_TEXTURE2D_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = bufferDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.ArraySize = bufferDesc.ArraySize;
		srvDesc.Texture2DArray.FirstArraySlice = 0;

		// TODO reduceMipLevelBy has only extremly small effect on performance, making it essentially pointless
		uint32_t mipLevels = bufferDesc.MipLevels;
		uint32_t reducedBy = std::min(reduceMipLevelBy, mipLevels - 1);
		srvDesc.Texture2DArray.MostDetailedMip = reducedBy;
		srvDesc.Texture2DArray.MipLevels = mipLevels - reducedBy;

		auto hr = d3d.device->CreateShaderResourceView(buffer, &srvDesc, targetSrv);
		::util::throwOnError(hr, "createTexture2dArraySrv failed!");
	}
}