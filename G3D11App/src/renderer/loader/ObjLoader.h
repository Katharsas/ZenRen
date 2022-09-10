#pragma once

#include "../Renderer.h"

namespace renderer::loader {
	std::unordered_map<std::string, std::vector<POS_NORMAL_UV>> loadObj(const std::string& inputFile);
}

