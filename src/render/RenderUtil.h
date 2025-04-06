#pragma once

#include "WinDx.h"
#include "Renderer.h"

namespace render::util
{
	void initViewport(BufferSize& size, D3D11_VIEWPORT* viewport);

	template <VERTEX_FEATURE F>
	void dumpVerts(const std::string& matName, const std::vector<VertexPos>& vertPos, const std::vector<F>& vertOther)
	{
		std::ostringstream buffer;
		buffer << matName << '\n';
		for (uint32_t i = 0; i < vertPos.size(); i++) {
			buffer << "    " << vertPos[i] << "  " << vertOther[i] << '\n';
		}
		LOG(INFO) << buffer.str();
	}

	std::string getVramUsage(IDXGIAdapter3* adapter);
	std::string getMemUsage();
}

