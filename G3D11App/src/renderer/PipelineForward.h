#pragma once

#include "dx11.h"
#include "RendererCommon.h"
#include "ShaderManager.h"
#include "Renderer.h"

namespace renderer::forward {
	void draw(D3d d3d, ShaderManager* shaders, RenderSettings& settings);
	ID3D11ShaderResourceView* initRenderBuffer(D3d d3d, BufferSize& size);
	void initViewport(BufferSize& size);
	void initDepthBuffer(D3d d3d, BufferSize& size, bool reverseZ);
	void initRasterizerStates(D3d d3d);
	void clean();
}

