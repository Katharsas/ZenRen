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

    void insert(VERTEX_DATA_BY_MAT& target, const ZenLoad::zCMaterialData& material, const vector<VERTEX_POS>& positions, const vector<VERTEX_OTHER>& other)
    {
        const auto& texture = material.texture;
        if (!texture.empty()) {
            Material material = { ::util::asciiToLower(texture) };
            insert(target, material, positions, other);
        }
    }

    void loadWorldMesh(
        VERTEX_DATA_BY_MAT& target,
        ZenLoad::zCMesh* worldMesh)
    {
        ZenLoad::PackedMesh packedMesh;
        worldMesh->packMesh(packedMesh, G_ASSET_RESCALE);

        for (const auto& submesh : packedMesh.subMeshes) {
            if (submesh.indices.empty()) {
                continue;
            }
            if (submesh.triangleLightmapIndices.empty()) {
                throw std::logic_error("Expected world mesh to have lightmap information!");
            }

            // at least in debug, resize & at-assignment is much faster than reserve & insert/copy, and we don't need temp array for flipping face order
            uint32_t indicesCount = submesh.indices.size();
            vector<VERTEX_POS> facesPos(indicesCount);
            vector<VERTEX_OTHER> facesOther(indicesCount);

            int32_t faceIndex = 0;
            for (uint32_t indicesIndex = 0; indicesIndex < indicesCount; indicesIndex += 3) {

                const array zenFace = getFaceVerts(packedMesh.vertices, submesh.indices, indicesIndex);

                ZenLoad::Lightmap lightmap;
                const int16_t faceLightmapIndex = submesh.triangleLightmapIndices[faceIndex];
                if (faceLightmapIndex != -1) {
                    lightmap = worldMesh->getLightmapReferences()[faceLightmapIndex];
                }

                for (uint32_t i = 0; i < 3; i++) {
                    const auto& zenVert = zenFace[i];
                    VERTEX_POS pos;
                    pos = from(zenVert.Position);
                    VERTEX_OTHER other;
                    other.normal = from(zenVert.Normal);
                    other.uvDiffuse = from(zenVert.TexCoord);
                    other.colLight = fromSRGB(D3DXCOLOR(zenVert.Color));
                    //other.colLight = D3DXCOLOR(1, 1, 1, 0.f);
                    other.dirLight = { -100.f, -100.f, -100.f };// value that is easy to check as not normalized in shader
                    other.lightSun = faceLightmapIndex == -1 ? 1.f : 0.f;

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
                        other.uvLightmap.i = (float) lightmap.lightmapTextureIndex;
                    }
                    // flip faces (seems like zEngine uses counter-clockwise winding, while we use clockwise winding)
                    // TODO use D3D11_RASTERIZER_DESC FrontCounterClockwise instead?
                    facesPos.at(indicesIndex + (2 - i)) = pos;
                    facesOther.at(indicesIndex + (2 - i)) = other;
                }
                faceIndex++;
            }

            insert(target, submesh.material, facesPos, facesOther);
        }
    }

    void posToXM4(const array<ZenLoad::WorldVertex, 3>& zenFace, array<XMVECTOR, 3>& target) {
        for (uint32_t i = 0; i < 3; i++) {
            const auto& zenVert = zenFace[i];
            target[i] = toXM4Pos(zenVert.Position);
        }
    }

    uint8_t normalToXM4(const array<ZenLoad::WorldVertex, 3>& zenFace, array<XMVECTOR, 3>& target, const XMVECTOR& faceNormalXm, bool debugChecksEnabled) {
        uint8_t zeroNormals = 0;
        for (uint32_t i = 0; i < 3; i++) {
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
                float normalFlatnessDegrees = normalFlatnessRadian * (180.f / 3.141592653589793238463f);
                if (normalFlatnessDegrees > 90) {
                    wrongNormals++;
                }
            }
        }
        return wrongNormals;
    }


    unordered_set<string> processedVisuals;

	void loadInstanceMesh(
        VERTEX_DATA_BY_MAT& target,
        const ZenLib::ZenLoad::zCProgMeshProto& mesh,
        const ZenLib::ZenLoad::PackedMesh packedMesh,
        const StaticInstance& instance,
        bool debugChecksEnabled)
    {
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

            // at least in debug, resize & at-assignment is much faster than reserve & insert/copy, and we don't need temp array for flipping face order
            uint32_t indicesCount = submesh.indices.size();
            vector<VERTEX_POS> facesPos(indicesCount);
            vector<VERTEX_OTHER> facesOther(indicesCount);

            for (uint32_t indicesIndex = 0; indicesIndex < indicesCount; indicesIndex += 3) {
                
                const array zenFace = getFaceVerts(packedMesh.vertices, submesh.indices, indicesIndex);

                // positions
                array<XMVECTOR, 3> posXm;
                posToXM4(zenFace, posXm);

                // normals
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
                for (uint32_t i = 0; i < 3; i++) {
                    const auto& zenVert = zenFace[i];
                    VERTEX_POS pos;
                    pos = transformPos(posXm[i], instance.transform);
                    VERTEX_OTHER other;
                    other.normal = transformDir(normalsXm[i], normalTransform, debugChecksEnabled);
                    other.uvDiffuse = from(zenVert.TexCoord);
                    other.uvLightmap = { 0, 0, -1 };
                    other.colLight = instance.colLightStatic;
                    other.dirLight = toVec3(XMVector3Normalize(instance.dirLightStatic));
                    other.lightSun = instance.receiveLightSun ? 1.f : 0.f;

                    // flip faces (seems like zEngine uses counter-clockwise winding, while we use clockwise winding)
                    // TODO use D3D11_RASTERIZER_DESC FrontCounterClockwise instead?
                    facesPos.at(indicesIndex + (2 - i)) = pos;
                    facesOther.at(indicesIndex + (2 - i)) = other;
                }
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

    void loadInstanceMeshLib(
        VERTEX_DATA_BY_MAT& target,
        const MeshLibData& libData,
        const StaticInstance& instance,
        bool debugChecksEnabled)
    {
        auto lib = libData.meshLib;
        {
            uint32_t count = 0;
            for (auto mesh : lib.getMeshes()) {
                auto trans = lib.getRootNodeTranslation();
                // It's unclear why negation is needed and if only z component or other components needs to be negated as well.
                // If z is not negated, sisha's in Gomez' throneroom are not placed correctly.
                XMMATRIX transformRoot = XMMatrixTranslationFromVector(XMVectorSet(-trans.x, -trans.y, -trans.z, 0));

                StaticInstance newInstance = instance;
                newInstance.transform = XMMatrixMultiply(transformRoot, instance.transform);

                loadInstanceMesh(target, mesh.getMesh(), libData.meshesPacked[count++], newInstance, debugChecksEnabled);
            }
        } {
            uint32_t count = 0;
            for (const auto& [name, mesh] : lib.getAttachments()) {
                auto index = lib.findNodeIndex(name);

                auto node = lib.getNodes()[index];
                XMMATRIX transform = toXMM(node.transformLocal);

                while (node.parentValid()) {
                    const auto& parent = lib.getNodes()[node.parentIndex];
                    XMMATRIX transformParent = toXMM(parent.transformLocal);
                    transform = XMMatrixMultiply(transform, transformParent);
                    node = parent;
                }
                // rootNode translation is not applied to attachments (example: see kettledrum placement on oldcamp music stage)

                StaticInstance newInstance = instance;
                newInstance.transform = XMMatrixMultiply(transform, instance.transform);

                loadInstanceMesh(target, mesh, libData.attachementsPacked[count++], newInstance, debugChecksEnabled);
            }
        }
    }
}