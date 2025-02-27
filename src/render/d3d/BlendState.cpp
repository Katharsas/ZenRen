#include "stdafx.h"
#include "BlendState.h"

#include "render/WinDx.h"

namespace render::d3d
{
	void createBlendState(D3d d3d, ID3D11BlendState** target, BlendType blendType, bool alphaToCoverage)
	{
		release(*target);

		if (blendType == BlendType::NONE && !alphaToCoverage) {
			*target = nullptr;// nullptr can be used as default blendState in D3D
			return;
		}

		D3D11_BLEND_DESC blendStateDesc = CD3D11_BLEND_DESC(CD3D11_DEFAULT{});
		blendStateDesc.AlphaToCoverageEnable = alphaToCoverage;

		auto& renderTarget = blendStateDesc.RenderTarget[0];
		if (blendType != BlendType::NONE) {
			assert(alphaToCoverage == false);

			renderTarget.BlendEnable = true;
			renderTarget.BlendOp = D3D11_BLEND_OP_ADD;

			renderTarget.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
			renderTarget.DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
			renderTarget.BlendOpAlpha = D3D11_BLEND_OP_ADD;

			switch (blendType) {
			case BlendType::ADD: {
				// TODO shading is still different from original... how are these actually shaded in Gothic?
				renderTarget.SrcBlend = D3D11_BLEND_SRC_ALPHA;
				renderTarget.DestBlend = D3D11_BLEND_ONE;
			} break;
			case BlendType::MULTIPLY: {
				renderTarget.SrcBlend = D3D11_BLEND_DEST_COLOR;
				renderTarget.DestBlend = D3D11_BLEND_SRC_COLOR;
			} break;
			case BlendType::BLEND_FACTOR: {
				renderTarget.SrcBlend = D3D11_BLEND_BLEND_FACTOR;
				renderTarget.DestBlend = D3D11_BLEND_INV_BLEND_FACTOR;
			} break;
			case BlendType::BLEND_ALPHA: {
				renderTarget.SrcBlend = D3D11_BLEND_SRC_ALPHA;
				renderTarget.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			} break;
			}
		}

		d3d.device->CreateBlendState(&blendStateDesc, target);
	}
}