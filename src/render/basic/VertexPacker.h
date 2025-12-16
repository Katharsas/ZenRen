#pragma once

#include "render/basic/Primitives.h"

namespace render
{
	uint32_t packNormal(Vec3 normal);
	Vec3 unpackNormal(uint32_t packed);
	uint32_t packUv(Uv uv);
	Uv unpackUv(uint32_t packed);

	std::pair<uint32_t, uint32_t> packLight(VertexLightType type, Color colLight, Uvi uviLightmap, uint16_t instanceId);
	VertexLightType unpackLightType(std::pair<uint32_t, uint32_t> packed);
	uint16_t unpackInstanceId(std::pair<uint32_t, uint32_t> packed);
	Color unpackLightColor(std::pair<uint32_t, uint32_t> packed);
	Uvi unpackLightmap(std::pair<uint32_t, uint32_t> packed);
}
