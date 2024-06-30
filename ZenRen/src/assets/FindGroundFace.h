#pragma once

#include "render/Common.h"
#include "LookupTrees.h"

namespace assets
{
	D3DXCOLOR interpolateColor(const VEC3& pos, const render::BATCHED_VERTEX_DATA& meshData, const render::VertKey& vertKey);
	std::optional<render::VertKey> getGroundFaceAtPos(const DirectX::XMVECTOR pos, const render::BATCHED_VERTEX_DATA& meshData, const VertLookupTree& vertLookup);
}

