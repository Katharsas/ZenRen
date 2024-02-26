#pragma once

#include "Renderer.h"

namespace renderer::util {
	void dumpVerts(const std::string& matName, const std::vector<POS>& vertPos, const std::vector<NORMAL_UV_LUV>& vertOther);
	std::string getVramUsage(IDXGIAdapter3* adapter);
}

