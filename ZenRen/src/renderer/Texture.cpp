#include "stdafx.h"
#include "Texture.h"

#include <comdef.h>

#include "RendererCommon.h"
#include "../Util.h"

#include "magic_enum/magic_enum.hpp"
#include "DirectXTex.h"

namespace renderer {
	using DirectX::ScratchImage;
	using DirectX::TexMetadata;
	using DirectX::DDSMetaData;

	const uint32_t MASK_DDPF_ALPHAPIXELS = 0x00000001;
	const std::array formatsNoAlpha = { DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM };
	const std::array formatsWithAlpha = { DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM };

	bool throwOnError(const HRESULT& hr, const std::string& message) {
		return util::throwOnError(hr, "Texture Load Error: " + message);
	}
	void throwError(const std::string& message) {
		util::throwError("Texture Load Error: " + message);
	}

	void resizeIfOtherSize(ScratchImage* image, int32_t width, int32_t height, const std::string& name) {
		const TexMetadata& metadata = image->GetMetadata();
		if (metadata.width != width || metadata.height != height) {

			// resize dicards any images after the first (resulting file will have no mipmaps)!
			ScratchImage imageResized;
			auto hr = Resize(*image->GetImage(0, 0, 0), width, height, DirectX::TEX_FILTER_DEFAULT, imageResized);
			throwOnError(hr, name);

			*image = std::move(imageResized);
		}
	}

	bool hasExpectedMipmapCount(const ScratchImage* image, bool isGothicZTex = false) {
		auto& metadata = image->GetMetadata();
		int32_t maxDimPixelCount = std::max(metadata.width, metadata.height);
		int32_t expectedCount = std::ceil(std::log2(maxDimPixelCount)) + 1;
		if (isGothicZTex) {
			expectedCount = std::max(1, expectedCount - 3);// compiled zTex textures seem to come without lowest 3 levels
		}
		return expectedCount == image->GetImageCount();
	}

	void createMipmapsIfMissing(ScratchImage* image, const std::string& name) {
		if (!hasExpectedMipmapCount(image)) {
			ScratchImage imageWithMips;
			auto hr = GenerateMipMaps(image->GetImages(), image->GetImageCount(), image->GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, imageWithMips);
			throwOnError(hr, name);

			*image = std::move(imageWithMips);
		}
	}

	bool hasAlphaData(const DDSMetaData& ddPixelFormat, const TexMetadata metadata, const std::string& name) {
		bool isCompressed = ddPixelFormat.fourCC != 0x0;
		if (!isCompressed) {
			return (ddPixelFormat.flags & MASK_DDPF_ALPHAPIXELS) == 1;
		}
		else {
			// for compressed textures, we just assume that certain formats are for non-alpha textures, because there is no way to find out without decompressing
			for (DXGI_FORMAT format : formatsNoAlpha) {
				if (metadata.format == format) {
					return false;
				}
			}
			for (DXGI_FORMAT format : formatsWithAlpha) {
				if (metadata.format == format) {
					return true;
				}
			}
			auto formatString = std::string(magic_enum::enum_name(metadata.format));
			throwError("Failed to determine if texture '" + name + "' uses alpha or not! Unrecognized format: " + formatString);
		}
	}

	ID3D11ShaderResourceView* createShaderTexArray(D3d d3d, std::vector<std::vector<uint8_t>>& ddsRaws, int32_t width, int32_t height, bool sRgb, bool noMip) {
		std::vector<DirectX::ScratchImage*> imageOwners;
		std::vector<DirectX::Image> images;

		DirectX::TexMetadata metadata;

		int32_t i = 0;
		for (auto& ddsRaw : ddsRaws) {
			std::string name = "lightmap_" + std::format("{:03}", i);
			DirectX::ScratchImage* image = new DirectX::ScratchImage();
			auto hr = DirectX::LoadFromDDSMemory(ddsRaw.data(), ddsRaw.size(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, &metadata, *image);
			throwOnError(hr, name);

			resizeIfOtherSize(image, width, height, name);
			if (!noMip) {
				// TODO it is unclear if Mimaps are needed since the lightmaps details are extremely low frequency and much likely will never noticably alias unless running at extremely low resolution
				createMipmapsIfMissing(image, name);
			}

			imageOwners.push_back(image);
			images.push_back(*image->GetImages());
			i++;
		}

		metadata.arraySize = images.size();

		// TODO
		// It is unclear if MipMaps work here.
		// We should instead write Image to DDS raw memory (see https://stackoverflow.com/questions/76144325/convert-any-input-dds-into-another-dds-format-in-directxtex)
		// and then load raw memory with CreateDDSTextureFromMemory (see https://github.com/Microsoft/DirectXTK/wiki/DDSTextureLoader)

		ID3D11ShaderResourceView* resourceView;
		auto sRgbFlag = sRgb ? DirectX::CREATETEX_FORCE_SRGB : DirectX::CREATETEX_IGNORE_SRGB;
		auto hr = DirectX::CreateShaderResourceViewEx(
			d3d.device, images.data(), images.size(), metadata,
			D3D11_USAGE::D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, sRgbFlag, &resourceView);

		throwOnError(hr, "lightmap_array_srv");

		for (auto& owner : imageOwners) {
			delete owner;
		}

		return resourceView;
	}

	void createSetSrv(D3d d3d, const ScratchImage& image, const std::string& name, bool sRgb, ID3D11ShaderResourceView** ppSrv) {
		auto sRgbFlag = sRgb ? DirectX::CREATETEX_FORCE_SRGB : DirectX::CREATETEX_IGNORE_SRGB;
		auto hr = DirectX::CreateShaderResourceViewEx(
			d3d.device, image.GetImages(), image.GetImageCount(), image.GetMetadata(),
			D3D11_USAGE::D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, sRgbFlag, ppSrv);

		throwOnError(hr, name);
	}

	Texture::Texture(D3d d3d, const std::string& sourceFile, bool sRgb)
	{
		auto name = sourceFile;
		util::asciiToLower(name);
		std::wstring sourceFileW = util::utf8ToWide(name);
		HRESULT hr;
		ScratchImage image;
		TexMetadata metadata;

		if (util::endsWith(name, ".tga")) {
			hr = LoadFromTGAFile(sourceFileW.c_str(), DirectX::TGA_FLAGS_ALLOW_ALL_ZERO_ALPHA, &metadata, image);
			throwOnError(hr, name);
		}
		else if (util::endsWith(name, ".png")) {
			hr = LoadFromWICFile(sourceFileW.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, image);
			throwOnError(hr, name);
		}

		createMipmapsIfMissing(&image, name);

		DirectX::TEX_ALPHA_MODE alphaMode = metadata.GetAlphaMode();
		hasAlpha_ = alphaMode != DirectX::TEX_ALPHA_MODE::TEX_ALPHA_MODE_OPAQUE;
		createSetSrv(d3d, image, name, sRgb, &resourceView);
	}

	Texture::Texture(D3d d3d, std::vector<uint8_t>& ddsRaw, bool isGothicZTex, const std::string& name, bool sRgb)
	{
		HRESULT hr;
		ScratchImage image;
		TexMetadata metadata;
		DDSMetaData ddPixelFormat;
		hr = DirectX::LoadFromDDSMemoryEx(ddsRaw.data(), ddsRaw.size(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, &metadata, &ddPixelFormat, image);
		throwOnError(hr, name);

		if (isGothicZTex) {
			// GPU does not support compressed zTex DDS format (DXGI_FORMAT_BC1_UNORM) as render target, so mipmap generation would fail.
			// TODO it might be better to decompress theses textures to allow mip map recreation, but that would likely take time.
			if (!hasExpectedMipmapCount(&image, true)) {
				LOG(WARNING) << "Texture Load Warning: Incorrect number of Mipmaps found! " << name;
			}
		}
		else {
			createMipmapsIfMissing(&image, name);
		}

		hasAlpha_ = hasAlphaData(ddPixelFormat, metadata, name);
		createSetSrv(d3d, image, name, sRgb, &resourceView);
	}

	Texture::~Texture()
	{
		release(resourceView);
	}

	ID3D11ShaderResourceView* Texture::GetResourceView()
	{
		return resourceView;
	}

	bool Texture::hasAlpha() {
		return hasAlpha_;
	}
}
