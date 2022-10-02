#include "stdafx.h"
#include "Texture.h"

#include <comdef.h>

#include "../Util.h"

#include "DirectXTex.h"

namespace renderer {

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

	Texture::Texture(D3d d3d, const std::string& sourceFile)
	{
		auto name = sourceFile;
		util::asciiToLowercase(name);
		std::wstring sourceFileW = util::utf8ToWide(name);
		HRESULT result;

		if (util::endsWith(name, ".tga")) {
			DirectX::ScratchImage image;
			DirectX::TexMetadata metadata;
			result = DirectX::LoadFromTGAFile(sourceFileW.c_str(), &metadata, image);

			DirectX::CreateShaderResourceView(d3d.device, image.GetImages(), image.GetImageCount(), metadata, &resourceView);
		}
		else if (util::endsWith(name, ".png")) {
			DirectX::ScratchImage image;
			DirectX::TexMetadata metadata;
			result = DirectX::LoadFromWICFile(sourceFileW.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, image);

			DirectX::CreateShaderResourceView(d3d.device, image.GetImages(), image.GetImageCount(), metadata, &resourceView);
		}
		else {
			result = 0;// DirectX::LoadFromWICFile
		}
		
		if (FAILED(result)) {
			_com_error err(result);
			std::wstring errorMessage = std::wstring(err.ErrorMessage());

			LOG(WARNING) << "Failed to load texture: " << sourceFile;
			LOG(WARNING) << util::wideToUtf8(errorMessage);
		}
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
