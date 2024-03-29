#pragma once

#include "dx11.h"
#include "Renderer.h"
#include "ShaderManager.h"

namespace renderer::world {
	void loadLevel(D3d d3d, std::string& level);
	void updateObjects();
	void updateShaderSettings(D3d d3d, RenderSettings& settings);
	void initLinearSampler(D3d d3d, RenderSettings& settings);
	void init(D3d d3d);
	void initConstantBufferPerObject(D3d d3d);
	void drawPrepass(D3d d3d, ShaderManager* shaders);
	void drawWorld(D3d d3d, ShaderManager* shaders);
	void drawWireframe(D3d d3d, ShaderManager* shaders);
	void clean();
}

