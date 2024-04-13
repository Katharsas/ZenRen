#pragma once

#include "../RendererCommon.h"
#include "VertPosLookup.h"

namespace renderer::loader
{
	bool rayIntersectsWorldFaces(DirectX::XMVECTOR rayStart, DirectX::XMVECTOR rayEnd, float maxDistance, const std::unordered_map<Material, VEC_VERTEX_DATA>& meshData, const VertLookupTree& vertLookup);
}