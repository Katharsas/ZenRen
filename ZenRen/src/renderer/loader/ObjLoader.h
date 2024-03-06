#pragma once

#include "../Renderer.h"

namespace renderer::loader {
	std::unordered_map<Material, VEC_VERTEX_DATA> loadObj(const std::string& inputFile);
}

