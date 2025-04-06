#pragma once

#include "render/Loader.h"

namespace assets
{
	void loadInstanceMeshBboxDebugVisual(render::MatToChunksToVertsBasic& target, const render::StaticInstance& instance);
	void loadPointDebugVisual(render::MatToChunksToVertsBasic& target, const Vec3& pos, const Vec3& scale = { 0.f, 0.f, 0.f }, const Color& color = Color(1, 0, 0, 1));
	void loadLineDebugVisual(render::MatToChunksToVertsBasic& target, const Vec3& posStart, Vec3& posEnd, const Color& color = Color(1, 0, 0, 1));
}