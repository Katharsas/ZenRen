#include "stdafx.h"
#include "Texture.h"

#include <comdef.h>

#include "RendererCommon.h"
#include "../Util.h"

#include "DirectXTex.h"

namespace renderer {
	using DirectX::ScratchImage;
	using DirectX::TexMetadata;

	bool throwOnError(const HRESULT& hr, const std::string& message) {
		return util::throwOnError(hr, "Texture Load Error: " + message);
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

	ID3D11ShaderResourceView* createShaderTexArray(D3d d3d, std::vector<std::vector<uint8_t>>& ddsRaws, int32_t width, int32_t height, bool noMip) {
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
		// It is unclear it MipMaps work here.
		// We should instead write Image to DDS raw memory (see https://stackoverflow.com/questions/76144325/convert-any-input-dds-into-another-dds-format-in-directxtex)
		// and then load raw memory with CreateDDSTextureFromMemory (see https://github.com/Microsoft/DirectXTK/wiki/DDSTextureLoader)

		ID3D11ShaderResourceView* resourceView;
		auto hr = DirectX::CreateShaderResourceView(d3d.device, images.data(), images.size(), metadata, &resourceView);
		throwOnError(hr, "lightmap_array_srv");

		for (auto& owner : imageOwners) {
			delete owner;
		}

		return resourceView;
	}

	Texture::Texture(D3d d3d, const std::string& sourceFile)
	{
		auto name = sourceFile;
		util::asciiToLower(name);
		std::wstring sourceFileW = util::utf8ToWide(name);
		HRESULT hr;
		ScratchImage image;
		TexMetadata metadata;

		if (util::endsWith(name, ".tga")) {
			hr = LoadFromTGAFile(sourceFileW.c_str(), &metadata, image);
			throwOnError(hr, name);
		}
		else if (util::endsWith(name, ".png")) {
			hr = LoadFromWICFile(sourceFileW.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, image);
			throwOnError(hr, name);
		}

		createMipmapsIfMissing(&image, name);

		hr = CreateShaderResourceView(d3d.device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), &resourceView);
		throwOnError(hr, name);
	}

	Texture::Texture(D3d d3d, std::vector<uint8_t>& ddsRaw, bool isGothicZTex, const std::string& name)
	{
		HRESULT hr;
		DirectX::ScratchImage image;
		DirectX::TexMetadata metadata;
		hr = DirectX::LoadFromDDSMemory(ddsRaw.data(), ddsRaw.size(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, &metadata, image);
		throwOnError(hr, name);

		if (isGothicZTex) {
			// GPU does not support zTex DDS format as render target, so mipmap generation would fail.
			if (!hasExpectedMipmapCount(&image, true)) {
				LOG(WARNING) << "Texture Load Warning: Incorrect number of Mipmaps found! " << name;
			}
		}
		else {
			createMipmapsIfMissing(&image, name);
		}

		hr = DirectX::CreateShaderResourceView(d3d.device, image.GetImages(), image.GetImageCount(), metadata, &resourceView);
		throwOnError(hr, name);
	}

	Texture::~Texture()
	{
		release(resourceView);
	}

	ID3D11ShaderResourceView* Texture::GetResourceView()
	{
		return resourceView;
	}
}
