#pragma once

#include <string>
#include "dx11.h"

namespace renderer {
	ID3D11ShaderResourceView* createShaderTexArray(D3d d3d, std::vector<std::vector<uint8_t>>& ddsRaws, int32_t width, int32_t height, bool noMip = false);

	class Texture
	{
	public:
		Texture(D3d d3d, const std::string& sourceFile, bool sRgb = true);
		Texture(D3d d3d, std::vector<uint8_t>& ddsRaw, bool isGothicZTex, const std::string& name, bool sRgb = true);
		~Texture();
		ID3D11ShaderResourceView* GetResourceView();
		bool hasAlpha();
	private:
		ID3D11ShaderResourceView* resourceView = nullptr;
		bool hasAlpha_ = false;
	};
}

