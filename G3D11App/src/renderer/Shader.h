#pragma once

#include <string>
#include "dx11.h"

namespace renderer {

	class Shader
	{
	public:
		struct VertexInputLayoutDesc {
			LPCSTR SemanticName;
			DXGI_FORMAT Format;
		};

		Shader(const std::string& sourceFile, const VertexInputLayoutDesc layout[], const int length,
			ID3D11Device * device);
		~Shader();
		ID3D11InputLayout * getVertexLayout();
		ID3D11VertexShader * getVertexShader();
		ID3D11PixelShader * getPixelShader();
	private:
		ID3D11InputLayout * vertexLayout;
		ID3D11VertexShader * vertexShader;
		ID3D11PixelShader * pixelShader;
	};

}