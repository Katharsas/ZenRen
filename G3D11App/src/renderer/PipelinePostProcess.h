#pragma once

#include "RendererCommon.h"
#include "ShaderManager.h"

namespace renderer::postprocess
{
	void draw(D3d d3d, ID3D11ShaderResourceView* linearBackBuffer, ShaderManager* shaders);
	void initBackBuffer(D3d d3d, IDXGISwapChain1* swapchain);
	void initViewport(BufferSize& size);
	void initLinearSampler(D3d d3d);
	void initVertexBuffers(D3d d3d, bool reverseZ);
	void clean();
}

