#include "stdafx.h"
#include "MeshFromVdfLoader.h"

#include <filesystem>

#include "render/MeshUtil.h"
#include "../Util.h"

namespace assets
{
    using namespace render;
	using namespace DirectX;
	using namespace ZenLib;
    using ::std::string;
    using ::std::array;
    using ::std::vector;
    using ::std::unordered_map;
    using ::std::unordered_set;
	using ::util::getOrCreate;

    const float zeroThreshold = 0.000001f;

    int32_t wrapModulo(int32_t positiveOrNegative, int32_t modulo)
    {
        return ((positiveOrNegative % modulo) + modulo) % modulo;
    }

    XMMATRIX inversedTransposed(const XMMATRIX& source) {
        const XMMATRIX transposed = XMMatrixTranspose(source);
        return XMMatrixInverse(nullptr, transposed);
    }

    VEC3 transformDir(const XMVECTOR& source, const XMMATRIX& transform, bool debugChecksEnabled = false)
    {
        if (debugChecksEnabled) {
            warnIfNotNormalized(source);
        }
        // XMVector3TransformNormal is hardcoded to use w=0, but might still output unnormalized normals when matrix has non-uniform scaling
        XMVECTOR result;
        result = XMVector3TransformNormal(source, transform);
        result = XMVector3Normalize(result);
        return toVec3(result);
    }

    VEC3 transformPos(const XMVECTOR& source, const XMMATRIX& transform)
    {
        XMVECTOR pos = XMVector4Transform(source, transform);
        return toVec3(pos);
    }

    const array<ZenLoad::WorldVertex, 3> getFaceVerts(
        const vector<ZenLoad::WorldVertex>& verts,
        const vector<uint32_t>& vertIndices,
        uint32_t currentIndex)
    {
        return {
            verts.at(vertIndices[currentIndex]),
            verts.at(vertIndices[currentIndex + 1]),
            verts.at(vertIndices[currentIndex + 2]),
        };
    }

    void insert(BATCHED_VERTEX_DATA& target, const ZenLoad::zCMaterialData& material, const vector<VERTEX_POS>& positions, const vector<VERTEX_OTHER>& other)
    {
        const auto& texture = material.texture;
        if (!texture.empty()) {
            Material material = { ::util::asciiToLower(texture) };
            insert(target, material, positions, other);
        }
    }

    void loadWorldMesh(
        BATCHED_VERTEX_DATA& target,
        ZenLoad::zCMesh* worldMesh)
    {
        ZenLoad::PackedMesh packedMesh;
        worldMesh->packMesh(packedMesh, 0.01f);

        for (const auto& submesh : packedMesh.subMeshes) {
            if (submesh.indices.empty()) {
                continue;
            }
            if (submesh.triangleLightmapIndices.empty()) {
                throw std::logic_error("Expected world mesh to have lightmap information!");
            }

            vector<VERTEX_POS> facesPos;
            vector<VERTEX_OTHER> facesOther;

            int32_t faceIndex = 0;
            for (int32_t indicesIndex = 0; indicesIndex < submesh.indices.size(); indicesIndex += 3) {

                const array zenFace = getFaceVerts(packedMesh.vertices, submesh.indices, indicesIndex);

                ZenLoad::Lightmap lightmap;
                const int16_t faceLightmapIndex = submesh.triangleLightmapIndices[faceIndex];
                if (faceLightmapIndex != -1) {
                    lightmap = worldMesh->getLightmapReferences()[faceLightmapIndex];
                }

                array<VERTEX_POS, 3> facePos;
                array<VERTEX_OTHER, 3> faceOther;

                for (int32_t i = 0; i < 3; i++) {
                    const auto& zenVert = zenFace[i];
                    VERTEX_POS pos;
                    pos = from(zenVert.Position);
                    VERTEX_OTHER other;
                    other.normal = from(zenVert.Normal);
                    other.uvDiffuse = from(zenVert.TexCoord);
                    other.colLight = fromSRGB(D3DXCOLOR(zenVert.Color));
                    //other.colLight = D3DXCOLOR(1, 1, 1, 0.f);
                    other.dirLight = { -100, -100, -100 };// value that is easy to check as not normalized in shader
                    other.lightSun = faceLightmapIndex == -1 ? 1 : 0;

                    if (faceLightmapIndex == -1) {
                        other.uvLightmap = { 0, 0, -1 };
                    }
                    else {
                        float unscale = 100;
                        XMVECTOR posXm = XMVectorSet(pos.x * unscale, pos.y * unscale, pos.z * unscale, 0);
                        XMVECTOR origin = XMVectorSet(lightmap.origin.x, lightmap.origin.y, lightmap.origin.z, 0);
                        XMVECTOR normalUp = XMVectorSet(lightmap.normalUp.x, lightmap.normalUp.y, lightmap.normalUp.z, 0);
                        XMVECTOR normalRight = XMVectorSet(lightmap.normalRight.x, lightmap.normalRight.y, lightmap.normalRight.z, 0);
                        XMVECTOR lightmapDir = posXm - origin;
                        other.uvLightmap.u = XMVectorGetX(XMVector3Dot(lightmapDir, normalRight));
                        other.uvLightmap.v = XMVectorGetX(XMVector3Dot(lightmapDir, normalUp));
                        other.uvLightmap.i = lightmap.lightmapTextureIndex;
                    }
                    // flip faces (seems like zEngine uses counter-clockwise winding, while we use clockwise winding)
                    // TODO use D3D11_RASTERIZER_DESC FrontCounterClockwise instead?
                    facePos.at(2 - i) = pos;
                    faceOther.at(2 - i) = other;
                }
                facesPos.insert(facesPos.end(), facePos.begin(), facePos.end());
                facesOther.insert(facesOther.end(), faceOther.begin(), faceOther.end());
                faceIndex++;
            }

            insert(target, submesh.material, facesPos, facesOther);
        }
    }

    void posToXM4(const array<ZenLoad::WorldVertex, 3>& zenFace, array<XMVECTOR, 3>& target) {
        for (int32_t i = 0; i < 3; i++) {
            const auto& zenVert = zenFace[i];
            target[i] = toXM4Pos(zenVert.Position);
        }
    }

    uint8_t normalToXM4(const array<ZenLoad::WorldVertex, 3>& zenFace, array<XMVECTOR, 3>& target, const XMVECTOR& faceNormalXm, bool debugChecksEnabled) {
        uint8_t zeroNormals = 0;
        for (int32_t i = 0; i < 3; i++) {
            const auto& zenVert = zenFace[i];
            if (debugChecksEnabled && isZero(zenVert.Normal, zeroThreshold)) {
                target[i] = faceNormalXm;
                zeroNormals++;
            }
            else {
                target[i] = toXM4Dir(zenVert.Normal);
            }
        }
        return zeroNormals;
    }

    uint8_t normalAngleCheck(const array<XMVECTOR, 3>& normalsXm, const XMVECTOR& faceNormalXm, bool debugChecksEnabled) {
        uint8_t wrongNormals = 0;
        if (debugChecksEnabled) {
            for (int32_t i = 0; i < 3; i++) {
                XMVECTOR angleXm = XMVector3AngleBetweenNormals(faceNormalXm, normalsXm[i]);
                float normalFlatnessRadian = XMVectorGetX(angleXm);
                float normalFlatnessDegrees = normalFlatnessRadian * (180.0 / 3.141592653589793238463);
                if (normalFlatnessDegrees > 90) {
                    wrongNormals++;
                }
            }
        }
        return wrongNormals;
    }


    unordered_set<string> processedVisuals;

	void loadInstanceMesh(
        BATCHED_VERTEX_DATA& target,
        const ZenLoad::zCProgMeshProto& mesh,
        const StaticInstance& instance,
        bool debugChecksEnabled)
    {
        ZenLoad::PackedMesh packedMesh;
        mesh.packMesh(packedMesh, 0.01f);

        XMMATRIX normalTransform = inversedTransposed(instance.transform);

        uint32_t totalNormals = 0;
        uint32_t zeroNormals = 0;
        uint32_t extremeNormals = 0;

        for (const auto& submesh : packedMesh.subMeshes) {
            if (submesh.indices.empty()) {
                continue;
            }
            if (!submesh.triangleLightmapIndices.empty()) {
                throw std::logic_error("Expected VOB mesh to NOT have lightmap information!");
            }

            vector<VERTEX_POS> facesPos;
            vector<VERTEX_OTHER> facesOther;

            for (uint32_t indicesIndex = 0; indicesIndex < submesh.indices.size(); indicesIndex += 3) {
                
                const array zenFace = getFaceVerts(packedMesh.vertices, submesh.indices, indicesIndex);
                array<VERTEX_POS, 3> facePos;
                array<VERTEX_OTHER, 3> faceOther;

                // read positions
                array<XMVECTOR, 3> posXm;
                posToXM4(zenFace, posXm);

                // read normals
                totalNormals += 3;
                XMVECTOR faceNormalXm;
                if (debugChecksEnabled) {
                    faceNormalXm = calcFlatFaceNormal(posXm);
                }
                array<XMVECTOR, 3> normalsXm;
                zeroNormals += normalToXM4(zenFace, normalsXm, faceNormalXm, debugChecksEnabled);

                // detect wrong normals
                extremeNormals += normalAngleCheck(normalsXm, faceNormalXm, debugChecksEnabled);

                // transform
                for (int32_t i = 0; i < 3; i++) {
                    const auto& zenVert = zenFace[i];
                    VERTEX_POS pos;
                    pos = transformPos(posXm[i], instance.transform);
                    VERTEX_OTHER other;
                    other.normal = transformDir(normalsXm[i], normalTransform, debugChecksEnabled);
                    other.uvDiffuse = from(zenVert.TexCoord);
                    other.uvLightmap = { 0, 0, -1 };
                    other.colLight = instance.colLightStatic;
                    other.dirLight = toVec3(XMVector3Normalize(instance.dirLightStatic));
                    other.lightSun = instance.receiveLightSun ? 1 : 0;

                    // flip faces (seems like zEngine uses counter-clockwise winding, while we use clockwise winding)
                    // TODO use D3D11_RASTERIZER_DESC FrontCounterClockwise instead?
                    facePos.at(2 - i) = pos;
                    faceOther.at(2 - i) = other;
                }
                facesPos.insert(facesPos.end(), facePos.begin(), facePos.end());
                facesOther.insert(facesOther.end(), faceOther.begin(), faceOther.end());
            }

            insert(target, submesh.material, facesPos, facesOther);
        }

        if (debugChecksEnabled) {
            const auto& visualname = instance.meshName;
            if (processedVisuals.find(visualname) == processedVisuals.end()) {
                if (zeroNormals > 0) {
                    LOG(WARNING) << "Normals: " << util::leftPad("'" + std::to_string(zeroNormals) + "/" + std::to_string(totalNormals), 9) << "' are ZERO:  " << visualname;
                }
                if (extremeNormals > 0) {
                    LOG(WARNING) << "Normals: " << util::leftPad("'" + std::to_string(extremeNormals) + "/" + std::to_string(totalNormals), 9) << "' are WRONG: " << visualname << " (over 90 degrees off recalc. flat normal)";
                }
            }
            processedVisuals.insert(visualname);
        }
	}
}