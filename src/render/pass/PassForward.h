#pragma once

#include "../WinDx.h"
#include "../Common.h"
#include "../Settings.h"

namespace render::pass::forward
{
	void clean();
	void draw(D3d d3d, const RenderSettings& settings, bool hasCameraChanged);
	void reinitShaders(D3d d3d);
	ID3D11ShaderResourceView* initRenderBuffer(D3d d3d, BufferSize& size, uint32_t multisampleCount);
	void initViewport(BufferSize& size);
	void initDepthBuffer(D3d d3d, BufferSize& size, uint32_t multisampleCount, bool reverseZ);
	void initRasterizerStates(D3d d3d, uint32_t multisampleCount, bool wireframe);
	void initBlendStates(D3d d3d, uint32_t multisampleCount, bool multisampleTransparency);
	void initConstantBuffers(D3d d3d);
}
