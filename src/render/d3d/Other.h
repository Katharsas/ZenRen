#pragma once

#include "render/WinDx.h"

namespace render::d3d
{
	static void createLinearSampler(D3d d3d, ID3D11SamplerState** target, uint16_t anisotropicLevel = 1)
	{
		release(*target);
		D3D11_SAMPLER_DESC samplerDesc = {};

		if (anisotropicLevel >= 2) {
			samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		}
		else {
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		}
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MaxAnisotropy = anisotropicLevel;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		d3d.device->CreateSamplerState(&samplerDesc, target);
	}
}