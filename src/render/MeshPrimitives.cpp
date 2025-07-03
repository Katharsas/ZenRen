#include "stdafx.h"
#include "MeshPrimitives.h"

#include "render/MeshUtil.h"

namespace render
{
    using namespace DirectX;
    using ::std::array;
    using ::std::vector;
    using ::std::pair;

    /**
     * Create quad.
     */
    template<Axis NORMAL, bool NEGATE>
    vector<array<PosUv, 3>> createQuadMesh(const Vec3& pos, const Vec2& size, const Uv& uv_offset, bool two_sided)
    {
        Uv uvA = Uv{ 0 + uv_offset.u, 0 + uv_offset.v };
        Uv uvB = Uv{ 1 + uv_offset.u, 0 + uv_offset.v };
        Uv uvC = Uv{ 1 + uv_offset.u, 1 + uv_offset.v };
        Uv uvD = Uv{ 0 + uv_offset.u, 1 + uv_offset.v };

        float flipSign = 1;
        if constexpr (NEGATE) {
            flipSign = -1;
        }

        PosUv A, B, C, D;
        if constexpr (NORMAL == Axis::Z) {
            A = { Vec3{ pos.x - flipSign * size.x, pos.y + size.y, pos.z }, uvA };
            B = { Vec3{ pos.x + flipSign * size.x, pos.y + size.y, pos.z }, uvB };
            C = { Vec3{ pos.x + flipSign * size.x, pos.y - size.y, pos.z }, uvC };
            D = { Vec3{ pos.x - flipSign * size.x, pos.y - size.y, pos.z }, uvD };
        }
        else if constexpr (NORMAL == Axis::Y) {
            A = { Vec3{ pos.x - size.x, pos.y, pos.z + flipSign * size.y }, uvA };
            B = { Vec3{ pos.x + size.x, pos.y, pos.z + flipSign * size.y }, uvB };
            C = { Vec3{ pos.x + size.x, pos.y, pos.z - flipSign * size.y }, uvC };
            D = { Vec3{ pos.x - size.x, pos.y, pos.z - flipSign * size.y }, uvD };
        }
        else {
            static_assert(false, "Not implemented!");
        }

        vector<array<PosUv, 3>> result;
        result.reserve(two_sided ? 4 : 2);

        // for indexing to work, vertices that can de deduplicated (A, C) must be adjacent to each other across faces
        result.push_back({ D, C, A });
        result.push_back({ A, C, B });
        if (two_sided) {
            result.push_back({ B, C, A });
            result.push_back({ A, C, D });
        }

        return result;
    }

    template vector<array<PosUv, 3>> createQuadMesh<Axis::Z, false>(const Vec3& pos, const Vec2& size, const Uv& uv_offset, bool two_sided);
}
