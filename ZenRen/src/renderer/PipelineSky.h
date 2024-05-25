#pragma once

#include "Renderer.h"
#include "ShaderManager.h"
#include "Sky.h"

namespace renderer::sky
{
	void loadSky(D3d d3d);
	void updateSkyLayers(D3d d3d, const std::array<SkyTexState, 2>& layerStates);
	void drawSky(D3d d3d, ShaderManager* shaders, const ShaderCbs& cbs, ID3D11SamplerState* layerSampler, const std::array<SkyTexState, 2>& layerStates);
	void initConstantBuffers(D3d d3d);
}

