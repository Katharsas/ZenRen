#pragma once

#include "../Renderer.h"

namespace renderer::loader {
	std::unordered_map<Material, VEC_POS_NORMAL_UV_LMUV> loadObj(const std::string& inputFile);
}

