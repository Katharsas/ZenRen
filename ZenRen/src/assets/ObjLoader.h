#pragma once

#include "render/Renderer.h"

namespace assets
{
	std::unordered_map<render::Material, render::VEC_VERTEX_DATA> loadObj(const std::string& inputFile);
}

