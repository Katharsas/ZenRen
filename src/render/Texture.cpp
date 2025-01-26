#include "stdafx.h"
#include "Texture.h"

#include <comdef.h>
#include "../Util.h"
#include <magic_enum.hpp>

#include "WinDx.h"
#include "DirectXTex.h"


namespace render {

	// TODO is using the mask correct?
	const uint32_t MASK_DDPF_ALPHAPIXELS = 0x00000001;

	const std::array formatsNoAlpha = { DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM };
	const std::array formatsWithAlpha = { DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM };

	Texture::Texture(TexInfo info, ID3D11ShaderResourceView* srv) : resourceView(srv), info(info) {}

	Texture::~Texture()
	{
		release(resourceView);
	}

	ID3D11ShaderResourceView* Texture::GetResourceView() const
	{
		return resourceView;
	}

	TexInfo Texture::getInfo() const {
		return info;
	}
}
