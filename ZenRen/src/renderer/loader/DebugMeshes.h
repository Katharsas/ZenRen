#pragma once

#include "../Renderer.h"

namespace renderer::loader
{
	void loadInstanceMeshBboxDebugVisual(std::unordered_map<Material, VEC_VERTEX_DATA>& target, const StaticInstance& instance);
	void loadPointDebugVisual(std::unordered_map<Material, VEC_VERTEX_DATA>& target, const VEC3& pos, const VEC3& scale = { 0.f, 0.f, 0.f });
}