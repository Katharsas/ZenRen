#pragma once

#include "../Renderer.h"
#include "../ShaderManager.h"
#include "../Sky.h"

namespace render::pass::sky
{
	void loadSky(D3d d3d);
	void updateSkyLayers(D3d d3d, const std::array<SkyTexState, 2>& layerStates, const Color& skyBackground, float timeOfDay, bool swapLayers);
	void drawSky(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs, ID3D11SamplerState* layerSampler);
	void initConstantBuffers(D3d d3d);
	void clean();
}

