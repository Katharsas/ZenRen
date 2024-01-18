#pragma once

#include "dx11.h"
#include "Renderer.h"
#include "ShaderManager.h"

namespace renderer::world {
	void updateObjects();
	void updateShaderSettings(D3d d3d, RenderSettings& settings);
	void initLinearSampler(D3d d3d, RenderSettings& settings);
	void initVertexIndexBuffers(D3d d3d);
	void initConstantBufferPerObject(D3d d3d);
	void draw(D3d d3d, ShaderManager* shaders);
	void clean();
}

