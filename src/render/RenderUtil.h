#pragma once

#include "WinDx.h"
#include "Renderer.h"

namespace render::util
{
	void initViewport(BufferSize& size, D3D11_VIEWPORT* viewport);

	void dumpVerts(const std::string& matName, const std::vector<VertexPos>& vertPos, const std::vector<NORMAL_UV_LUV>& vertOther);
	std::string getVramUsage(IDXGIAdapter3* adapter);
	std::string getMemUsage();
}

