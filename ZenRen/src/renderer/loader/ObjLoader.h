#pragma once

#include "../Renderer.h"

namespace renderer::loader {
	std::unordered_map<Material, std::vector<WORLD_VERTEX>> loadObj(const std::string& inputFile);
}

