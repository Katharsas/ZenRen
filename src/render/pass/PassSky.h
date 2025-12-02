#pragma once

#include "render/Dx.h"
#include "render/Sky.h"

namespace render::pass::sky
{
	void loadSky(D3d d3d);
	void updateSkyLayers(D3d d3d, const std::array<SkyTexState, 2>& layerStates, const Color& skyBackground, float timeOfDay, bool swapLayers);
	void drawSky(D3d d3d, ID3D11SamplerState* layerSampler);
	void reinitShaders(D3d d3d);
	void initConstantBuffers(D3d d3d);
	void clean();
}

