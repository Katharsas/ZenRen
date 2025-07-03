#pragma once

#include "render/Primitives.h"

namespace render
{
    enum class Axis {
        X, Y, Z
    };

    template<Axis NORMAL, bool NEGATE = false>
	std::vector<std::array<PosUv, 3>> createQuadMesh(const Vec3& pos, const Vec2& size, const Uv& uv_offset, bool two_sided);
}