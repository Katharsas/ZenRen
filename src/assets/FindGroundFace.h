#pragma once

#include "render/Common.h"
#include "LookupTrees.h"

namespace assets
{
	COLOR interpolateColor(const VEC3& pos, const render::MatToChunksToVertsBasic& meshData, const render::VertKey& vertKey);
	std::optional<render::VertKey> getGroundFaceAtPos(const DirectX::XMVECTOR pos, const render::MatToChunksToVertsBasic& meshData, const VertLookupTree& vertLookup);
}

