#pragma once

#include "render/Dx.h"
#include "render/Common.h"

// TODO rename d3d global variable (local variables are unaffected)
namespace render::d3d
{
	void createBlendState(D3d d3d, ID3D11BlendState** target, BlendType blendType = BlendType::NONE, bool alphaToCoverage = false);
}