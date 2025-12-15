#pragma once

#include "render/basic/Common.h"
#include "LookupTrees.h"

namespace assets
{
	Color interpolateColor(const Vec3& pos, const render::MatToChunksToVertsBasic& meshData, const render::VertKey& vertKey);
	std::optional<render::VertKey> getGroundFaceAtPos(const DirectX::XMVECTOR pos, const render::MatToChunksToVertsBasic& meshData, const VertLookupTree& vertLookup);
}

