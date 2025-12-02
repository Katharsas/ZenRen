#pragma once

#include <dxgiformat.h>

#include "render/Dx.h"
#include "render/Graphics.h"

namespace render::d3d
{
	struct VertexAttributeDesc {
		DXGI_FORMAT format;
		uint32_t bufferSlot = 0;
		std::string const * semanticName;
		uint32_t semanticIndex = 0;
	};

	struct Shader {
		ID3D11InputLayout* vertexLayout = nullptr;
		ID3D11VertexShader* vertexShader = nullptr;
		ID3D11PixelShader* pixelShader = nullptr;

		void release() {
			render::release(vertexLayout);
			render::release(vertexShader);
			render::release(pixelShader);
		}
	};

	std::vector<VertexAttributeDesc> buildInputLayoutDesc(const std::initializer_list<VertexAttributes>& buffers);

	void createShader(D3d d3d, Shader& target, const std::string& sourceFile, const std::vector<VertexAttributeDesc>& inputLayout, bool vsOnly = false);
}