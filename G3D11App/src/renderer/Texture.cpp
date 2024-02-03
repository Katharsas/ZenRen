#include "stdafx.h"
#include "Texture.h"

#include <comdef.h>

#include "../Util.h"

#include "DirectXTex.h"

namespace renderer {
	using DirectX::ScratchImage;
	using DirectX::TexMetadata;

	bool warnOnError(const HRESULT& hr, const std::string& message) {
		return util::warnOnError(hr, "Texture Load Error: " + message);
	}

	ID3D11ShaderResourceView* createShaderTexArray(D3d d3d, std::vector<std::vector<uint8_t>>& ddsRaws, int32_t width, int32_t height) {
		std::vector<DirectX::ScratchImage*> imageOwners;
		std::vector<DirectX::Image> images;
		images.reserve(ddsRaws.size());

		for (auto& ddsRaw : ddsRaws) {
			DirectX::ScratchImage* image = new DirectX::ScratchImage;
			DirectX::TexMetadata metadata;
			HRESULT result = DirectX::LoadFromDDSMemory(ddsRaw.data(), ddsRaw.size(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, &metadata, *image);
			if (metadata.width != width || metadata.height != height) {
				DirectX::ScratchImage* imageResized = new DirectX::ScratchImage;
				DirectX::Resize(*image->GetImage(0, 0, 0), width, height, DirectX::TEX_FILTER_DEFAULT, *imageResized);
				images.push_back(*imageResized->GetImage(0, 0, 0));
				delete image;
				image = imageResized;
			}
			else {
				images.push_back(*image->GetImage(0, 0, 0));
			}
			imageOwners.push_back(image);
		}

		DirectX::ScratchImage image;// = new DirectX::ScratchImage;
		image.InitializeArrayFromImages(images.data(), images.size());

		ID3D11ShaderResourceView* resourceView;
		DirectX::CreateShaderResourceView(d3d.device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), &resourceView);

		for (auto& owner : imageOwners) {
			delete owner;
		}

		return resourceView;
	}

	void createMipmapsIfMissing(ScratchImage* image, std::string& name) {
		const TexMetadata& metadata = image->GetMetadata();
		uint32_t maxPixelCount = std::max(metadata.width, metadata.height);
		uint32_t expectedMipCount = std::ceil(std::log2(maxPixelCount)) + 1;
		if (expectedMipCount != image->GetImageCount()) {

			ScratchImage imageWithMips;
			auto hr = GenerateMipMaps(image->GetImages(), image->GetImageCount(), image->GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, imageWithMips);
			warnOnError(hr, name);

			*image = std::move(imageWithMips);
		}
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
			warnOnError(hr, name);
		}
		else if (util::endsWith(name, ".png")) {
			hr = LoadFromWICFile(sourceFileW.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, image);
			warnOnError(hr, name);
		}

		createMipmapsIfMissing(&image, name);

		hr = CreateShaderResourceView(d3d.device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), &resourceView);
		warnOnError(hr, name);
	}

	Texture::Texture(D3d d3d, std::vector<uint8_t>& ddsRaw)
	{
		DirectX::ScratchImage image;
		DirectX::TexMetadata metadata;
		HRESULT result = DirectX::LoadFromDDSMemory(ddsRaw.data(), ddsRaw.size(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, &metadata, image);
		
		DirectX::CreateShaderResourceView(d3d.device, image.GetImages(), image.GetImageCount(), metadata, &resourceView);
		
		if (FAILED(result)) {
			LOG(WARNING) << "Failed to load DDS texture from memory!";
		}
	}

	Texture::~Texture()
	{
		if (resourceView != nullptr) {
			resourceView->Release();
		}
	}

	ID3D11ShaderResourceView* Texture::GetResourceView()
	{
		return resourceView;
	}
}
