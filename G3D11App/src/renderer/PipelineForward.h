#pragma once

#include "dx11.h"
#include "RendererCommon.h"
#include "ShaderManager.h"
#include "Renderer.h"

namespace renderer::forward {
	void clean();
	void draw(D3d d3d, ShaderManager* shaders, RenderSettings& settings);
	ID3D11ShaderResourceView* initRenderBuffer(D3d d3d, BufferSize& size, uint32_t multisampleCount);
	void initViewport(BufferSize& size);
	void initDepthBuffer(D3d d3d, BufferSize& size, uint32_t multisampleCount, bool reverseZ);
	void initRasterizerStates(D3d d3d, uint32_t multisampleCount, bool wireframe);
}

