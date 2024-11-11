#pragma once

#include <string>
#include "WinDx.h"
#include "Common.h"

namespace render
{
	ID3D11ShaderResourceView* createShaderTexArray(D3d d3d, std::vector<std::vector<uint8_t>>& ddsRaws, int32_t width, int32_t height, bool sRgb = true, bool noMip = false);

	// Currently, this class only provides access to a texture through a ID3D11ShaderResourceView pointer.
	// To get the actual ID3D11Texture2D interface, you would have to call GetResourceView()->GetResource(&target), which returns
	// a ID3D11Resource, which could be cast to ID3D11Texture2D (?). The ID3D11Resource must also always be released manually after that.
	// So maybe it would be better to just provide a ID3D11Texture2D pointer here, so callers don't have to worry about that stuff.
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

