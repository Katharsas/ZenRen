#pragma once

#include "render/Primitives.h"
#include "render/Dx.h"

#include <dxgiformat.h>

namespace render::d3d
{
	struct SurfaceInfo {
		uint32_t bytesPerRow = 0;
		uint32_t rowCount = 0;
	};

	struct InitialData {
		std::uint8_t * dataPtr;
		SurfaceInfo surface;
	};

	BufferSize getTexture2dSize(ID3D11Texture2D* buffer);

	SurfaceInfo calcSurfaceInfo(BufferSize size, DXGI_FORMAT format);
	SurfaceInfo calcSurfaceInfoDxtCompressed(BufferSize size, DXGI_FORMAT format);

	void createTexture2dBuf(
		D3d d3d, ID3D11Texture2D** target, BufferSize size, DXGI_FORMAT format, uint16_t sampleCount = 1,
		BufferUsage usage = BufferUsage::IMMUTABLE, bool writeUnordered = false, std::vector<InitialData> initialMips = {});

	void createTexture2dBuf(
		D3d d3d, ID3D11Texture2D** target, BufferSize size, DXGI_FORMAT format,
		BufferUsage usage = BufferUsage::IMMUTABLE, bool writeUnordered = false);

	void createTexture2dBuf(
		D3d d3d, ID3D11Texture2D** target, BufferSize size, DXGI_FORMAT format, std::vector<InitialData> initialMips,
		BufferUsage usage = BufferUsage::IMMUTABLE);

	void createTexture2dSrv(D3d d3d, ID3D11ShaderResourceView** targetSrv, ID3D11Texture2D* buffer);
	void createTexture2dUav(D3d d3d, ID3D11UnorderedAccessView** targetUav, ID3D11Texture2D* buffer);

	void createTexture2dBufForReadbackCopy(D3d d3d, ID3D11Texture2D** target, ID3D11Texture2D* originalBuffer);

	template<typename T>
	void readbackTexture2d(D3d d3d, ID3D11Texture2D* buffer, std::function<bool(T* row, uint16_t rowIndex, uint16_t count)> processPixelRow);

	void createTexture2dArrayBufByCopy(D3d d3d, ID3D11Texture2D** target, std::vector<ID3D11Texture2D*> buffersToCopy, BufferUsage usage);
	void createTexture2dArraySrv(D3d d3d, ID3D11ShaderResourceView** targetSrv, ID3D11Texture2D* buffer);
}