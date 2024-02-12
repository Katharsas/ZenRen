#pragma once

#include "Renderer.h"

namespace renderer::util {
	void dumpVerts(std::string& matName, std::vector<POS_NORMAL_UV>& verts);
	std::string getVramUsage(IDXGIAdapter3* adapter);
}

