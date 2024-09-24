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

    template<bool transform>
    void posToXM4(const array<ZenLoad::WorldVertex, 3>& zenFace, array<XMVECTOR, 3>& target, const XMMATRIX& posTransform = XMMatrixIdentity()) {
        for (uint32_t i = 0; i < 3; i++) {
            const auto& zenVert = zenFace[i];
            if (transform) {
                target[i] = XMVector4Transform(toXM4Pos(zenVert.Position), posTransform);
            }
            else {
                target[i] = toXM4Pos(zenVert.Position);
            }
        }
    }

    struct NormalsStats {
        uint32_t total = 0;
        uint32_t zero = 0;
        uint32_t extreme = 0;

        auto operator+=(const NormalsStats& rhs)
        {
            total += rhs.total;
            zero += rhs.zero;
            extreme += rhs.extreme;
        };
    };

    void logNormalStats(const NormalsStats& normalStats, const std::string& meshName)
    {
        if (normalStats.zero > 0) {
            LOG(WARNING) << "Normals: " << util::leftPad("'" + std::to_string(normalStats.zero) + "/" + std::to_string(normalStats.total), 9) << "' are ZERO:  " << meshName;
        }
        if (normalStats.extreme > 0) {
            LOG(WARNING) << "Normals: " << util::leftPad("'" + std::to_string(normalStats.extreme) + "/" + std::to_string(normalStats.total), 9) << "' are WRONG: " << meshName << " (over 90 degrees off recalc. flat normal)";
        }
    }

    void transformDir(XMVECTOR& target, const XMVECTOR& source, const XMMATRIX& transform)
    {
        // XMVector3TransformNormal is hardcoded to use w=0, but might still output unnormalized normals when matrix has non-uniform scaling
        target = XMVector3TransformNormal(source, transform);
        target = XMVector3Normalize(target);
    }

    template<bool transform, bool debugChecksEnabled>
    NormalsStats normalToXM4(array<XMVECTOR, 3>& target, const array<ZenLoad::WorldVertex, 3>& zenFace, const array<XMVECTOR, 3>& facePosXm, const XMMATRIX& normalTransform = XMMatrixIdentity())
    {
        NormalsStats normalStats = { .total = 3 };

        XMVECTOR flatNormalXm;
        if (debugChecksEnabled) {
            flatNormalXm = calcFlatFaceNormal(facePosXm);
        }

        for (uint32_t i = 0; i < 3; i++) {
            const auto& zenVert = zenFace[i];
            if (debugChecksEnabled && isZero(zenVert.Normal, zeroThreshold)) {
                target[i] = flatNormalXm;
                normalStats.zero++;
            }
            else {
                target[i] = toXM4Dir(zenVert.Normal);
                if (debugChecksEnabled) {
                    warnIfNotNormalized(target[i]);
                }
                if (transform) {
                    transformDir(target[i], target[i], normalTransform);
                }
            }

            if (debugChecksEnabled) {
                XMVECTOR angleXm = XMVector3AngleBetweenNormals(flatNormalXm, target[i]);
                float normalFlatnessRadian = XMVectorGetX(angleXm);
                float normalFlatnessDegrees = normalFlatnessRadian * (180.f / 3.141592653589793238463f);
                if (normalFlatnessDegrees > 90) {
                    normalStats.extreme++;
                }
            }
        }

        return normalStats;
    }


    Material toMaterial(const ZenLoad::zCMaterialData& material)
    {
        const auto& texture = material.texture;
        return { getTexId(::util::asciiToLower(texture)) };
    }

    void insertFace(VEC_VERTEX_DATA& target, const array<VERTEX_POS, 3>& facePos, const array<VERTEX_OTHER, 3>& faceOther) {
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
        ZenLoad::zCMesh* worldMesh,
        bool debugChecksEnabled, uint32_t loadIndex)
    {
        ZenLoad::PackedMesh packedMesh;
        worldMesh->packMesh(packedMesh, G_ASSET_RESCALE);

        LOG(DEBUG) << "World Mesh: Loading " << packedMesh.subMeshes.size() << " Sub-Meshes.";

        NormalsStats normalStats;

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
                posToXM4<false>(zenFace, facePosXm);

                // normals
                array<XMVECTOR, 3> faceNormalsXm;
                if (debugChecksEnabled) {
                    normalStats += normalToXM4<false, true>(faceNormalsXm, zenFace, facePosXm);
                }
                else {
                    normalStats += normalToXM4<false, false>(faceNormalsXm, zenFace, facePosXm);
                }

                array<VERTEX_POS, 3> facePos;
                array<VERTEX_OTHER, 3> faceOther;

                for (uint32_t i = 0; i < 3; i++) {
                    const auto& zenVert = zenFace[i];
                    VERTEX_POS pos = toVec3(facePosXm[i]);
                    VERTEX_OTHER other;
                    other.normal = toVec3(faceNormalsXm[i]);
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
                VEC_VERTEX_DATA& chunkData = ::util::getOrCreateDefault(materialData, chunkIndex);
                insertFace(chunkData, facePos, faceOther);

                faceIndex++;
            }
        }

        if (debugChecksEnabled) {
            logNormalStats(normalStats, "LEVEL_MESH");
        }
    }

    void loadWorldMesh(
        VERT_CHUNKS_BY_MAT& target,
        ZenLoad::zCMesh* worldMesh,
        bool debugChecksEnabled)
    {
        uint32_t instances = 1;
        for (uint32_t i = 0; i < instances; ++i) {
            loadWorldMesh(target, worldMesh, i, debugChecksEnabled);
        }
    }

    XMMATRIX inversedTransposed(const XMMATRIX& source) {
        const XMMATRIX transposed = XMMatrixTranspose(source);
        return XMMatrixInverse(nullptr, transposed);
    }

    unordered_set<string> processedVisuals;

	void loadInstanceMesh(
        VERT_CHUNKS_BY_MAT& target,
        const ZenLib::ZenLoad::zCProgMeshProto& mesh,
        const ZenLib::ZenLoad::PackedMesh packedMesh,
        const StaticInstance& instance,
        bool debugChecksEnabled)
    {
        // TODO switch to chunked loading
        // TODO share code with world mesh loading if possible
        XMMATRIX normalTransform = inversedTransposed(instance.transform);

        NormalsStats normalStats;

        for (const auto& submesh : packedMesh.subMeshes) {
            if (isSubmeshEmptyAndValidate(submesh, false)) {
                continue;
            }

            uint32_t indicesCount = submesh.indices.size();

            const Material& mat = toMaterial(submesh.material);
            unordered_map<ChunkIndex, VEC_VERTEX_DATA>& materialData = ::util::getOrCreateDefault(target, mat);

            for (uint32_t indicesIndex = 0; indicesIndex < indicesCount; indicesIndex += 3) {
                
                const array zenFace = getFaceVerts(packedMesh.vertices, submesh.indices, indicesIndex);

                // positions
                array<XMVECTOR, 3> facePosXm;
                posToXM4<true>(zenFace, facePosXm, instance.transform);

                // normals
                array<XMVECTOR, 3> faceNormalsXm;
                if (debugChecksEnabled) {
                    normalStats += normalToXM4<true, true>(faceNormalsXm, zenFace, facePosXm, instance.transform);
                }
                else {
                    normalStats += normalToXM4<true, false>(faceNormalsXm, zenFace, facePosXm, instance.transform);
                }

                array<VERTEX_POS, 3> facePos;
                array<VERTEX_OTHER, 3> faceOther;

                // transform
                for (uint32_t i = 0; i < 3; i++) {
                    const auto& zenVert = zenFace[i];
                    VERTEX_POS pos = toVec3(facePosXm[i]);
                    VERTEX_OTHER other;
                    other.normal = toVec3(faceNormalsXm[i]);
                    other.uvDiffuse = from(zenVert.TexCoord);
                    other.uvLightmap = { 0, 0, -1 };
                    other.colLight = instance.colLightStatic;
                    other.dirLight = toVec3(XMVector3Normalize(instance.dirLightStatic));
                    other.lightSun = instance.receiveLightSun ? 1.f : 0.f;

                    // flip faces (seems like zEngine uses counter-clockwise winding, while we use clockwise winding)
                    // TODO use D3D11_RASTERIZER_DESC FrontCounterClockwise instead?
                    facePos.at(2 - i) = pos;
                    faceOther.at(2 - i) = other;
                }

                const ChunkIndex chunkIndex = toChunkIndex(centroidPos(facePosXm));
                VEC_VERTEX_DATA& chunkData = ::util::getOrCreateDefault(materialData, chunkIndex);
                insertFace(chunkData, facePos, faceOther);
            }
        }

        if (debugChecksEnabled) {
            const auto& visualname = instance.meshName;
            logNormalStats(normalStats, visualname);
            processedVisuals.insert(visualname);
        }
	}

    void loadInstanceMeshLib(
        VERT_CHUNKS_BY_MAT& target,
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