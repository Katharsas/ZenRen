#include "stdafx.h"
#include "render/basic/MeshUtil.h"

#include <filesystem>

#include "Util.h"

namespace render
{
    using namespace DirectX;
    using std::array;

    DirectX::XMMATRIX toXMMatrix(const float * matrix)
    {
        return XMMATRIX(matrix);
    }

    XMFLOAT3 toFloat3(const XMVECTOR& xm4)
    {
        XMFLOAT3 result;
        XMStoreFloat3(&result, xm4);
        return result;
    }

    Vec3 toVec3(const XMFLOAT3& xmf3)
    {
        return Vec3{ xmf3.x, xmf3.y, xmf3.z };
    }
    Vec3 toVec3(const XMVECTOR& xm4)
    {
        XMFLOAT4 result;
        XMStoreFloat4(&result, xm4);
        return Vec3{ result.x, result.y, result.z };
    }
    Vec4 toVec4(const XMVECTOR& xm4)
    {
        XMFLOAT4 result;
        XMStoreFloat4(&result, xm4);
        return Vec4{ result.x, result.y, result.z, result.w };
    }

    bool isZero(const XMVECTOR& vec, float threshold)
    {
        XMVECTOR almostZero = DirectX::XMVectorReplicate(threshold);
        uint32_t comparisonResult;
        XMVectorEqualR(&comparisonResult, XMVectorGreater(vec, almostZero), DirectX::XMVectorZero());
        return XMComparisonAllTrue(comparisonResult);
    }

    XMVECTOR bboxCenter(const array<XMVECTOR, 2>& bbox)
    {
        return 0.5f * (bbox[0] + bbox[1]);
    }

    XMVECTOR centroidPos(const array<XMVECTOR, 3>& posXm)
    {
        const static float oneThird = 1.f / 3.f;
        return XMVectorScale(XMVectorAdd(XMVectorAdd(posXm[0], posXm[1]), posXm[2]), oneThird);
    }

    XMVECTOR calcFlatFaceNormal(const array<XMVECTOR, 3>& posXm)
    {
        // counter-clockwise winding, flip XMVector3Cross argument order for clockwise winding
        return  XMVector3Normalize(XMVector3Cross(XMVectorSubtract(posXm[2], posXm[0]), XMVectorSubtract(posXm[1], posXm[0])));
    }

    GridPos toGridPos(const grid::Grid& grid, XMVECTOR posXm)
    {
        Vec3 pos = toVec3(posXm);
        return grid::getGridPosForPoint(grid, { pos.x, pos.z });
    }

    void warnIfNotNormalized(const XMVECTOR& source)
    {
        XMVECTOR normalized = XMVector3Normalize(source);
        XMVECTOR nearEqualMask = XMVectorNearEqual(source, normalized, XMVectorReplicate(.0001f));
        XMVECTOR nearEqualXm = XMVectorSelect(XMVectorReplicate(1.f), XMVectorReplicate(.0f), nearEqualMask);
        Vec4 nearEqual = toVec4(nearEqualXm);
        if (nearEqual.x != 0 || nearEqual.y != 0 || nearEqual.z != 0 || nearEqual.w != 0) {
            LOG(INFO) << "Vector was not normalized! " << source << "  |  " << normalized << "  |  " << nearEqualXm;
        }
    }

    void printXMMatrix(const XMMATRIX& matrix, const std::string& numberFormat)
    {
        // Example numberFormat = :7.2f
        XMFLOAT4X4 tmp;
        XMStoreFloat4x4(&tmp, matrix);
        LOG(DEBUG) << "Matrix:";
        std::string format = "    {" + numberFormat + "}, {" + numberFormat + "}, {" + numberFormat + "}, {" + numberFormat + "}";
        LOG(DEBUG) << std::vformat(format, std::make_format_args(tmp._11, tmp._12, tmp._13, tmp._14));
        LOG(DEBUG) << std::vformat(format, std::make_format_args(tmp._21, tmp._22, tmp._23, tmp._24));
        LOG(DEBUG) << std::vformat(format, std::make_format_args(tmp._31, tmp._32, tmp._33, tmp._34));
        LOG(DEBUG) << std::vformat(format, std::make_format_args(tmp._41, tmp._42, tmp._43, tmp._44));
    }
}