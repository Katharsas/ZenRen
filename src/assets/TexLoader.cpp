#include "stdafx.h"
#include "TexLoader.h"

#include "Util.h"
#include "render/d3d/TextureBuffer.h"
#include "render/WinDx.h"

#include "assets/AssetFinder.h"

#include "DirectXTex.h"
#include "magic_enum.hpp"
//#include "zenload/ztex2dds.h"

#undef ERROR
#include "zenkit/Texture.hh"

namespace assets
{
    using namespace render;
    using ::std::vector;

	struct FormatInfo {
		DXGI_FORMAT dxgi = DXGI_FORMAT_UNKNOWN;
		bool hasAlpha = false;
	};

	bool throwOnError(const HRESULT& hr, const std::string& message) {
		return ::util::throwOnError(hr, "Texture Load Error: " + message);
	}
	void throwError(const std::string& message) {
		::util::throwError("Texture Load Error: " + message);
	}

	BufferSize getSize(const DirectX::TexMetadata& metadata)
	{
		return {
			.width = (uint16_t)metadata.width,
			.height = (uint16_t)metadata.height,
		};
	}

	d3d::SurfaceInfo getSurfaceInfo(const DirectX::Image * mipPtr)
	{
		BufferSize size = {
			.width = (uint16_t)mipPtr->width,
			.height = (uint16_t)mipPtr->height,
		};
		return d3d::calcSurfaceInfo(size, mipPtr->format);
	}

	vector<d3d::InitialData> getInitialData(const DirectX::ScratchImage& image)
	{
		vector<d3d::InitialData> initialData;
		initialData.reserve(image.GetImageCount());
		for (uint32_t i = 0; i < image.GetImageCount(); i++) {
			auto * mip = image.GetImage(i, 0, 0);
			initialData.push_back({ mip->pixels, getSurfaceInfo(mip) });
		}
		return initialData;
	}

	d3d::SurfaceInfo getSurfaceInfo(const zenkit::Texture& tex, uint32_t mipLevel, DXGI_FORMAT format)
	{
		BufferSize size = {
			.width = (uint16_t)tex.mipmap_width(mipLevel),
			.height = (uint16_t)tex.mipmap_height(mipLevel),
		};
		return d3d::calcSurfaceInfo(size, format);
	}

	vector<d3d::InitialData> getInitialData(
		const zenkit::Texture& tex, DXGI_FORMAT format, bool decompress, vector<vector<uint8_t>>& decompressedDataOut)
	{
		decompressedDataOut.reserve(tex.mipmaps());
		vector<d3d::InitialData> initialData;
		initialData.reserve(tex.mipmaps());
		if (decompress) {
			for (uint32_t i = 0; i < tex.mipmaps(); i++) {
				decompressedDataOut.emplace_back(tex.as_rgba8(i));
				initialData.push_back({ decompressedDataOut.back().data(), getSurfaceInfo(tex, i, format) });
			}
		}
		else {
			for (uint32_t i = 0; i < tex.mipmaps(); i++) {
				initialData.push_back({ (uint8_t*)tex.data(i).data(), getSurfaceInfo(tex, i, format) });
			}
		}
		return initialData;
	}

	FormatInfo getDxgiFormatIfSupported(zenkit::TextureFormat format, bool srgb)
	{
		FormatInfo info;
		switch (format) {
		case zenkit::TextureFormat::DXT1: {
			info.dxgi = srgb ? DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM;
		} break;
		case zenkit::TextureFormat::DXT3: {
			info.dxgi = srgb ? DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM_SRGB : DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM;
			info.hasAlpha = true;
		} break;
		//case zenkit::TextureFormat::DXT5: format = DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM_SRGB; hasAlpha = true; break;
		}
		return info;
	}

	bool hasEnoughMipmaps(BufferSize size, uint32_t mipCount) {
		// Gothic appears to be using the smaller side to determine how many mipmaps to generate
		// todo change to min
		int32_t smallDimPixelCount = std::min(size.width, size.height);
		int32_t maxExpectedMipCount = (int)std::ceil(std::log2(smallDimPixelCount)) + 1;

		// compiled zTex textures seem to come without lowest 3 levels
		int32_t minExpectedMipCount = std::max(1, maxExpectedMipCount - 3);
		return mipCount >= minExpectedMipCount;
	}

	DirectX::Image createDirectXTexImage(BufferSize size, DXGI_FORMAT format, d3d::SurfaceInfo surface, const uint8_t* dataPtr)
	{
		return {
			.width = size.width,
			.height = size.height,
			.format = format,
			.rowPitch = surface.bytesPerRow,
			.slicePitch = surface.bytesPerRow * surface.rowCount,
			.pixels = (uint8_t*)dataPtr,
		};
	}


	void createMipmaps(DirectX::ScratchImage& imageAndTarget, const std::string name)
	{
		DirectX::ScratchImage imageWithMips;
		auto hr = GenerateMipMaps(
			imageAndTarget.GetImages(), imageAndTarget.GetImageCount(), imageAndTarget.GetMetadata(),
			DirectX::TEX_FILTER_DEFAULT, 0, imageWithMips);
		throwOnError(hr, name);
		imageAndTarget = std::move(imageWithMips);
	}

	Texture* createTexture(D3d d3d, BufferSize size, FormatInfo format, bool srgb, const vector<d3d::InitialData>& initialData)
	{
		ID3D11Texture2D* buffer = nullptr;
		d3d::createTexture2dBuf(d3d, &buffer, size, format.dxgi, initialData);
		ID3D11ShaderResourceView* srv = nullptr;
		d3d::createTexture2dSrv(d3d, &srv, buffer);
		release(buffer);

		TexInfo info = {
			.width = size.width,
			.height = size.height,
			.mipLevels = (uint16_t)initialData.size(),
			.hasAlpha = format.hasAlpha,
			.format = (uint32_t)format.dxgi,
			.srgb = srgb,
		};

		return new Texture(info, srv);
	}

	Texture* createTextureFromImageFormat(D3d d3d, const FileData& imageFile, bool srgb)
	{
		std::string name = ::util::asciiToLower(imageFile.name);
		HRESULT hr;
		DirectX::ScratchImage image;
		DirectX::TexMetadata metadata;

		if (::util::endsWith(name, ".tga")) {
			DirectX::TGA_FLAGS flags = DirectX::TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA;
			if (srgb) {
				flags |= DirectX::TGA_FLAGS_DEFAULT_SRGB;
			}
			hr = DirectX::LoadFromTGAMemory(imageFile.data, imageFile.size, flags, &metadata, image);
			throwOnError(hr, name);
		}
		else if (::util::endsWith(name, ".png")) {
			DirectX::WIC_FLAGS flags = DirectX::WIC_FLAGS_NONE;
			if (srgb) {
				flags |= DirectX::WIC_FLAGS_DEFAULT_SRGB;
			}
			hr = DirectX::LoadFromWICMemory(imageFile.data, imageFile.size, flags, &metadata, image);
			throwOnError(hr, name);
		}
		else {
			throwError("Texture file format not supported!");
		}

		if (metadata.arraySize > 1) {
			throwError("Texture files with mipmaps or layers are not supported!");
		}
		createMipmaps(image, name);

		FormatInfo format = {
			.dxgi = metadata.format,
			.hasAlpha = metadata.GetAlphaMode() != DirectX::TEX_ALPHA_MODE::TEX_ALPHA_MODE_OPAQUE,
		};
		vector<d3d::InitialData> initialData = getInitialData(image);
		return createTexture(d3d, getSize(metadata), format, srgb, initialData);
	}

	Texture* createDefaultTexture(D3d d3d)
	{
		// TODO maybe interal assets should be cached as well in AssetCache
		FileData data = assets::getData(assets::getInternal(AssetsIntern::DEFAULT_TEXTURE));
		return createTextureFromImageFormat(d3d, data, true);
	}

	Texture* createTextureFromGothicTex(D3d d3d, const zenkit::Texture& tex, const std::string name, bool srgb, std::optional<BufferSize> targetSizeOpt)
	{
		bool decompress = false;
		FormatInfo format = getDxgiFormatIfSupported(tex.format(), srgb);

		if (format.dxgi == DXGI_FORMAT_UNKNOWN) {
			if (tex.format() == zenkit::TextureFormat::R5G6B5) {
				// basically only one G1 sky texture and lightmaps are R5G6B5 but what can you do
				decompress = true;
				format = {
					.dxgi = srgb ? DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM,
					.hasAlpha = false
				};
			}
			else {
				LOG(WARNING) << "Texture Load: Failed to load TEX because of unsupported format!";
				return assets::createDefaultTexture(d3d);
			}
		}

		BufferSize size = { tex.width(), tex.height() };

		bool resize = targetSizeOpt.has_value() && size != targetSizeOpt.value();
		bool rebuild = resize || !hasEnoughMipmaps(size, tex.mipmaps());

		// when resizing we always re-generate all mips since that is easier and probably cleaner.
		if (rebuild) {
			if (resize) {
				LOG(INFO) << "Texture Load: Rebuilding texture for resize: " << name;
			} else {
				static const std::string lightmapPrefix = "lightmap";// lightmaps never have mipmaps, so logging it is noise
				if (!util::startsWith(name, lightmapPrefix)) {
					LOG(INFO) << "Texture Load: Rebuilding texture due to missing mipmaps: " << name;
				}
			}
			
			format.dxgi = srgb ? DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
			d3d::SurfaceInfo surface = d3d::calcSurfaceInfo(size, format.dxgi);
			auto uncompressedMipZero = tex.as_rgba8(0);
			DirectX::Image mipZero = createDirectXTexImage(size, format.dxgi, surface, uncompressedMipZero.data());

			DirectX::ScratchImage image;
			if (resize) {
				auto targetSize = targetSizeOpt.value();
				auto hr = Resize(mipZero, targetSize.width, targetSize.height, DirectX::TEX_FILTER_DEFAULT, image);
				throwOnError(hr, name);
				size = targetSize;
			}
			else {
				image.InitializeFromImage(mipZero);
			}
			
			createMipmaps(image, name);

			vector<d3d::InitialData> initialData = getInitialData(image);
			return createTexture(d3d, size, format, srgb, initialData);
		}
		else {
			// decompressedData must be kept alive until texture is created
			vector<vector<uint8_t>> decompressedMips;
			vector<d3d::InitialData> initialData = getInitialData(tex, format.dxgi, decompress, decompressedMips);
			return createTexture(d3d, size, format, srgb, initialData);
		}
	}

	Texture* createTextureFromGothicTex(D3d d3d, const FileData& file, bool srgb)
	{
		auto name = ::util::asciiToLower(file.name);
		assert(::util::endsWith(name, ".tex"));

		zenkit::Texture tex = {};
		auto read = zenkit::Read::from(file.data, file.size);
		tex.load(read.get());

		return createTextureFromGothicTex(d3d, tex, name, srgb, std::nullopt);
	}

	Texture* createTextureOrDefault(D3d d3d, const std::string& assetName, bool srgb)
	{
		// TODO consider passing TexId instead of assetName

		auto opt = assets::getIfAnyExists(assetName, FORMATS_TEXTURE);
		if (opt.has_value()) {
			auto& [handle, ext] = opt.value();
			auto data = assets::getData(handle);
			if (ext.str() == FormatsCompiled::TEX.str()) {
				return assets::createTextureFromGothicTex(d3d, data, srgb);
			}
			else {
				return assets::createTextureFromImageFormat(d3d, data, srgb);
			}
		}
		else {
			return createDefaultTexture(d3d);
		}
	}

	BufferSize getMaxSize(const vector<zenkit::Texture>& textures)
	{
		BufferSize currentMax = { 0, 0 };
		for (auto& tex : textures) {
			currentMax.width = std::max(currentMax.width, (uint16_t)tex.width());
			currentMax.height = std::max(currentMax.height, (uint16_t)tex.height());
		}
		return currentMax;
	}

	vector<Texture*> createTexturesFromLightmaps(D3d d3d, const vector<FileData>& lightmapFiles)
	{
		vector<zenkit::Texture> textures;
		textures.reserve(lightmapFiles.size());
		for (auto& file : lightmapFiles) {
			textures.emplace_back();
			auto read = zenkit::Read::from(file.data, file.size);
			textures.back().load(read.get());
		}

		const BufferSize& targetSize = getMaxSize(textures);

		LOG(INFO) << "Texture Load: Rebuilding all lightmaps due to missing mipmaps!";
		LOG(INFO) << "Texture Load: Resizing smaller lightmaps to target size " << targetSize << "!";

		vector<Texture*> result;
		result.reserve(textures.size());
		for (uint32_t i = 0; i < textures.size(); i++) {
			auto& name = lightmapFiles.at(i).name;
			result.push_back(createTextureFromGothicTex(d3d, textures.at(i), name, true, targetSize));
		}
		return result;
	}
}
