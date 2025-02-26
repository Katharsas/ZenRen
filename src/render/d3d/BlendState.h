#pragma once

#include "render/Dx.h"

namespace render
{
	// TODO disable depth writing
	// TODO if face order can result in different outcome, we would need to sort these by distance 
	// to camera theoretically (see waterfall near G2 start)
	enum class BlendType : uint8_t {
		NONE = 0,         // normal opaque or alpha-tested, shaded
		BLEND_ALPHA = 1,  // alpha channel blending, ??
		BLEND_FACTOR = 2, // fixed factor blending (water), shaded (?)
		ADD = 3,          // alpha channel additive blending
		MULTIPLY = 4,     // color multiplication, linear color textures, no shading
	};
}
namespace render::dx
{
	void createBlendState(D3d d3d, ID3D11BlendState** target, BlendType blendType = BlendType::NONE, bool alphaToCoverage = false);
}