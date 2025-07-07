#pragma once

#include "render/Dx.h"

namespace render::d3d
{
	void createSampler(D3d d3d, ID3D11SamplerState** target, uint16_t anisotropicLevel = 1);
}