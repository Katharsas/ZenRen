#pragma once

#include "render/Loader.h"

namespace assets
{
	void loadInstanceMeshBboxDebugVisual(render::VERT_CHUNKS_BY_MAT& target, const render::StaticInstance& instance);
	void loadPointDebugVisual(render::VERT_CHUNKS_BY_MAT& target, const VEC3& pos, const VEC3& scale = { 0.f, 0.f, 0.f }, const COLOR& color = COLOR(1, 0, 0, 1));
	void loadLineDebugVisual(render::VERT_CHUNKS_BY_MAT& target, const VEC3& posStart, VEC3& posEnd, const COLOR& color = COLOR(1, 0, 0, 1));
}