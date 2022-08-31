#include "stdafx.h"
#include "Texture.h"

#include <comdef.h>

#include "../Util.h"

#include "DirectXTex.h"

namespace renderer {
	Texture::Texture(D3d d3d, const std::string& sourceFile)
	{
		std::wstring sourceFileW = util::utf8ToWide(sourceFile);
		HRESULT result;

		if (util::endsWith(sourceFile, ".tga") || util::endsWith(sourceFile, ".TGA")) {
			DirectX::ScratchImage image;
			DirectX::TexMetadata metadata;
			result = DirectX::LoadFromTGAFile(sourceFileW.c_str(), &metadata, image);

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
