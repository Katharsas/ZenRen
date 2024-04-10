#pragma once

#include "../RendererCommon.h"
#include "VertPosLookup.h"

namespace renderer::loader
{
	D3DXCOLOR interpolateColor(const VEC3& pos, const std::unordered_map<Material, VEC_VERTEX_DATA>& meshData, const VertKey& vertKey);
	std::optional<VertKey> getGroundFaceAtPos(const DirectX::XMVECTOR pos, const std::unordered_map<Material, VEC_VERTEX_DATA>& meshData, const VertLookupTree& vertLookup);
}

