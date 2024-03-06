#pragma once

#include "../RendererCommon.h"
#include "VertPosLookup.h"

namespace renderer::loader
{
	std::optional<D3DXCOLOR> getLightStaticAtPos(const DirectX::XMVECTOR pos, const std::unordered_map<Material, VEC_VERTEX_DATA>& meshData, const SpatialCache& spatialCache);
}

