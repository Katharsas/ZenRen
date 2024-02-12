#pragma once

#include <string>
#include "dx11.h"

namespace renderer {
	ID3D11ShaderResourceView* createShaderTexArray(D3d d3d, std::vector<std::vector<uint8_t>>& ddsRaws, int32_t width, int32_t height);

	class Texture
	{
	public:
		Texture(D3d d3d, const std::string& sourceFile);
		Texture(D3d d3d, std::vector<uint8_t>& ddsRaw);
		~Texture();
		ID3D11ShaderResourceView* GetResourceView();
	private:
		ID3D11ShaderResourceView* resourceView = nullptr;
	};
}

