#include "stdafx.h"
#include "Texture.h"

#include "../Util.h"

namespace renderer {
	Texture::Texture(D3d d3d, const std::string& sourceFile)
	{
		resourceView = nullptr;
		std::wstring sourceFileW = util::utf8ToWide(sourceFile);
		D3DX11CreateShaderResourceViewFromFileW(d3d.device, sourceFileW.c_str(), nullptr, nullptr, &resourceView, nullptr);
	}

	Texture::~Texture()
	{
		resourceView->Release();
	}

	ID3D11ShaderResourceView* Texture::GetResourceView()
	{
		return resourceView;
	}
}
