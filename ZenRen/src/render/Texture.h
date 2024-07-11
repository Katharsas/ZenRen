#pragma once

#include <string>
#include "dx11.h"
#include "Common.h"

namespace render
{
	ID3D11ShaderResourceView* createShaderTexArray(D3d d3d, std::vector<std::vector<uint8_t>>& ddsRaws, int32_t width, int32_t height, bool sRgb = true, bool noMip = false);

	class Texture
	{
	public:
		Texture(D3d d3d, const std::string& sourceFile, bool sRgb = true);
		Texture(D3d d3d, std::vector<uint8_t>& ddsRaw, bool isGothicZTex, const std::string& name, bool sRgb = true);
		~Texture();
		ID3D11ShaderResourceView* GetResourceView() const;
		TexInfo getInfo() const;
	private:
		ID3D11ShaderResourceView* resourceView = nullptr;
		TexInfo info;
	};
}

