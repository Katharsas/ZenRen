#include "stdafx.h"
#include "MeshFromVdfLoader.h"

#include <filesystem>

#include "AssetCache.h"
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

    void posToXM4(const array<ZenLoad::WorldVertex, 3>& zenFace, array<XMVECTOR, 3>& target) {
        for (uint32_t i = 0; i < 3; i++) {
            const auto& zenVert = zenFace[i];
            target[i] = toXM4Pos(zenVert.Position);
        }
    }

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


    Material toMaterial(const ZenLoad::zCMaterialData& material)
    {
        const auto& texture = material.texture;
        return { getTexId(::util::asciiToLower(texture)) };
    }

    void insertFace(VEC_VERTEX_DATA& target, const array<VERTEX_POS, 3>& facePos, const array<VERTEX_OTHER, 3>& faceOther) {
        // TODO find fastest way to insert in debug mode
        target.vecPos.push_back(facePos[0]);
        target.vecPos.push_back(facePos[1]);
        target.vecPos.push_back(facePos[2]);
        target.vecOther.push_back(faceOther[0]);
        target.vecOther.push_back(faceOther[1]);
        target.vecOther.push_back(faceOther[2]);
    }

    array<VEC3, 2> createVertlistBbox(const vector<VERTEX_POS>& verts)
    {
        BoundingBox bbox;
        BoundingBox::CreateFromPoints(bbox, verts.size(), (XMFLOAT3*) verts.data(), sizeof(VEC3));
        array<XMFLOAT3, 8> corners;
        bbox.GetCorners(corners.data());
        XMFLOAT3 min = corners[2];
        XMFLOAT3 max = corners[4];

        return { toVec3(min), toVec3(max) };
    }

    XMVECTOR centroidPos(array<XMVECTOR, 3> face)
    {
        const static float oneThird = 1.f / 3.f;
        return XMVectorScale(XMVectorAdd(XMVectorAdd(face[0], face[1]), face[2]), oneThird);
    }

    ChunkIndex toChunkIndex(XMVECTOR posXm)
    {
        const static XMVECTOR chunkSize = toXM4Pos(VEC3{ chunkSizePerDim, chunkSizePerDim, chunkSizePerDim });
        XMVECTOR indexXm = XMVectorFloor(XMVectorDivide(posXm, chunkSize));

        XMFLOAT4 result;
        XMStoreFloat4(&result, indexXm);

        return { (int16_t) result.x, (int16_t) result.z };
    }

    bool isSubmeshEmptyAndValidate(const ZenLoad::PackedMesh::SubMesh& submesh, bool expectLigtmaps)
    {
        if (submesh.material.texture.empty()) {
            return true;
        }
        if (submesh.indices.empty()) {
            return true;
        }
        if (expectLigtmaps) {
            if (submesh.triangleLightmapIndices.empty()) {
                ::util::throwError("Expected world mesh to have lightmap information!");
            }
            if (submesh.triangleLightmapIndices.size() != (submesh.indices.size() / 3)) {
                ::util::throwError("Expected one lightmap index per face (with value -1 for non-lightmapped faces).");
            }
        }
        else {
            if (!submesh.triangleLightmapIndices.empty()) {
                ::util::throwError("Expected VOB mesh to NOT have lightmap information!");
            }
        }
        return false;
    }

    void loadWorldMesh(
        VERT_CHUNKS_BY_MAT& target,
        ZenLoad::zCMesh* worldMesh, uint32_t loadIndex)
    {
        ZenLoad::PackedMesh packedMesh;
        worldMesh->packMesh(packedMesh, G_ASSET_RESCALE);

        LOG(DEBUG) << "World Mesh: Loading " << packedMesh.subMeshes.size() << " Sub-Meshes.";

        for (const auto& submesh : packedMesh.subMeshes) {
            if (isSubmeshEmptyAndValidate(submesh, true)) {
                continue;
            }

            uint32_t indicesCount = submesh.indices.size();

            const Material& mat = toMaterial(submesh.material);
            unordered_map<ChunkIndex, VEC_VERTEX_DATA>& materialData = ::util::getOrCreateDefault(target, mat);

            int32_t faceIndex = 0;
            for (uint32_t indicesIndex = 0; indicesIndex < indicesCount; indicesIndex += 3) {

                const array zenFace = getFaceVerts(packedMesh.vertices, submesh.indices, indicesIndex);

                ZenLoad::Lightmap lightmap;
                const int16_t lightmapIndex = submesh.triangleLightmapIndices[faceIndex];
                bool hasLightmap = lightmapIndex != -1;
                if (hasLightmap) {
                    lightmap = worldMesh->getLightmapReferences()[lightmapIndex];
                }

                array<XMVECTOR, 3> facePosXm;
                posToXM4(zenFace, facePosXm);

                array<VERTEX_POS, 3> facePos;
                array<VERTEX_OTHER, 3> faceOther;

                for (uint32_t i = 0; i < 3; i++) {
                    const auto& zenVert = zenFace[i];
                    VERTEX_POS pos;
                    pos = toVec3(facePosXm[i]);
                    VERTEX_OTHER other;
                    other.normal = from(zenVert.Normal);
                    other.uvDiffuse = from(zenVert.TexCoord);
                    other.colLight = fromSRGB(D3DXCOLOR(zenVert.Color));
                    //other.colLight = D3DXCOLOR(1, 1, 1, 0.f);
                    other.dirLight = { -100.f, -100.f, -100.f };// value that is easy to check as not normalized in shader
                    other.lightSun = hasLightmap ? 0.f : 1.0f;

                    if (!hasLightmap) {
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
                    facePos.at(2 - i) = pos;
                    faceOther.at(2 - i) = other;
                }

                const ChunkIndex chunkIndex = toChunkIndex(centroidPos(facePosXm));
                VEC_VERTEX_DATA& chunkData = ::util::getOrCreateDefault(materialData, chunkIndex);// this function is faster for getting existing
                insertFace(chunkData, facePos, faceOther);

                faceIndex++;
            }
        }
    }

    void loadWorldMesh(
        VERT_CHUNKS_BY_MAT& target,
        ZenLoad::zCMesh* worldMesh)
    {
        uint32_t instances = 1;
        for (uint32_t i = 0; i < instances; ++i) {
            loadWorldMesh(target, worldMesh, i);
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
        // TODO switch to chunked loading
        // TODO share code with world mesh loading if possible
        XMMATRIX normalTransform = inversedTransposed(instance.transform);

        uint32_t totalNormals = 0;
        uint32_t zeroNormals = 0;
        uint32_t extremeNormals = 0;

        for (const auto& submesh : packedMesh.subMeshes) {
            if (isSubmeshEmptyAndValidate(submesh, false)) {
                continue;
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

            const Material& material = toMaterial(submesh.material);
            insert(target, material, facesPos, facesOther);
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