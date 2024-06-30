#pragma once

#include "render/Common.h"
#include "LookupTrees.h"

namespace assets
{
	D3DXCOLOR interpolateColor(const VEC3& pos, const render::VERTEX_DATA_BY_MAT& meshData, const render::VertKey& vertKey);
	std::optional<render::VertKey> getGroundFaceAtPos(const DirectX::XMVECTOR pos, const render::VERTEX_DATA_BY_MAT& meshData, const VertLookupTree& vertLookup);
}

