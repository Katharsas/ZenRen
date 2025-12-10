#pragma once

#include "render/Primitives.h"

namespace render
{
	uint32_t packNormal(Vec3 normal);
	Vec3 unpackNormal(uint32_t packed);
	uint32_t packUv(Uv uv);
	Uv unpackUv(uint32_t packed);
}
