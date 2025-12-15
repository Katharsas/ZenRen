#pragma once

#include "Dx.h"
#include "render/basic/Common.h"
#include "assets/AssetFinder.h"

#include <string>

namespace render
{
	// Currently, this class only provides access to a texture through a ID3D11ShaderResourceView pointer.
	// To get the actual ID3D11Texture2D interface, you would have to call GetResourceView()->GetResource(&target), which returns
	// a ID3D11Resource, which could be cast to ID3D11Texture2D (?). The ID3D11Resource must also always be released manually after that.
	// So maybe it would be better to just provide a ID3D11Texture2D pointer here, so callers don't have to worry about that stuff.
	class Texture
	{
	public:
		Texture(TexInfo info, ID3D11ShaderResourceView* srv);
		~Texture();
		ID3D11ShaderResourceView* GetResourceView() const;
		TexInfo getInfo() const;
	private:
		ID3D11ShaderResourceView* resourceView = nullptr;
		TexInfo info;
	};
}

