#pragma once

#include <d3d11.h>

#include "D3D11Renderer.h"
#include "ShaderManager.h"

namespace renderer::postprocess
{
	void draw(D3d d3d, ID3D11ShaderResourceView* linearBackBuffer, ShaderManager* shaders);
	void initBackBuffer(D3d d3d, IDXGISwapChain* swapchain);
	void initLinearSampler(D3d d3d);
	void initVertexBuffers(D3d d3d, bool reverseZ);
	void reInitVertexBuffers(D3d d3d, bool reverseZ);
	void clean();
}

