#pragma once

#include <string>
#include "dx11.h"

namespace renderer {

	struct VertexInputLayoutDesc {
		LPCSTR SemanticName;
		UINT SemanticIndex = 0;
		DXGI_FORMAT Format;
		UINT InputSlot = 0;
	};

	class Shader
	{
	public:
		Shader(D3d d3d, const std::string& sourceFile, const std::vector<VertexInputLayoutDesc>& layoutDesc, bool vsOnly = false);
		~Shader();
		ID3D11InputLayout* getVertexLayout();
		ID3D11VertexShader* getVertexShader();
		ID3D11PixelShader* getPixelShader();
	private:
		ID3D11InputLayout* vertexLayout = nullptr;
		ID3D11VertexShader* vertexShader = nullptr;
		ID3D11PixelShader* pixelShader = nullptr;
	};

}