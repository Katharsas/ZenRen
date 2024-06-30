#pragma once

#include "../Common.h"
#include "../Settings.h"
#include "../ShaderManager.h"

namespace render::pass::post
{
	void clean();
	void draw(D3d d3d, ID3D11ShaderResourceView* linearBackBuffer, ShaderManager* shaders, const RenderSettings& settings);
	void resolveAndPresent(D3d d3d, IDXGISwapChain1* swapchain);
	void initDownsampleBuffers(D3d d3d, BufferSize& size);
	void initDepthBuffer(D3d d3d, BufferSize& size);
	void initBackBuffer(D3d d3d, IDXGISwapChain1* swapchain);
	void initViewport(BufferSize& size);
	void initLinearSampler(D3d d3d, bool pointSampling);
	void initVertexBuffers(D3d d3d, bool reverseZ);
	void initConstantBuffers(D3d d3d);
}

