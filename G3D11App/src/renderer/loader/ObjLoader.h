#pragma once

#include "../Renderer.h"

namespace renderer::loader {
	std::unordered_map<std::string, std::vector<POS_NORMAL_UV_COL>> loadObj(const std::string& inputFile);
}

