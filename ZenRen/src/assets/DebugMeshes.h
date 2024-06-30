#pragma once

#include "render/Renderer.h"

namespace assets
{
	void loadInstanceMeshBboxDebugVisual(render::VERTEX_DATA_BY_MAT& target, const render::StaticInstance& instance);
	void loadPointDebugVisual(render::VERTEX_DATA_BY_MAT& target, const VEC3& pos, const VEC3& scale = { 0.f, 0.f, 0.f }, const D3DXCOLOR& color = D3DXCOLOR(1, 0, 0, 1));
	void loadLineDebugVisual(render::VERTEX_DATA_BY_MAT& target, const VEC3& posStart, VEC3& posEnd, const D3DXCOLOR& color = D3DXCOLOR(1, 0, 0, 1));
}