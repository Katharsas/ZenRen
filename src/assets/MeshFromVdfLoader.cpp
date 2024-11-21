#include "stdafx.h"
#include "MeshFromVdfLoader.h"

#include <type_traits>
#include <filesystem>

#include "AssetCache.h"
#include "render/MeshUtil.h"
#include "../Util.h"

#include "magic_enum.hpp"
#include "glm/gtc/type_ptr.hpp"

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
    const XMMATRIX identity = XMMatrixIdentity();
    struct Unused {};

    static_assert(Vec3<glm::vec3>);
    static_assert(Vec3<ZMath::float3>);

    static_assert(Vec2<glm::vec2>);
    static_assert(Vec2<ZMath::float2>);

    // TODO Prepare for index buffer creation:
    // We should really at some point switch to processing/converting all vertex positions/features first
    // and THEN validating correct values for each face. Current approach is not really compatible with index buffers.


    // ###########################################################################
    // POSITIONS
    // ###########################################################################

    // POSITIONS - VECTOR ACCESSORS

    template <typename Mesh, typename Submesh>
    using GetPos = XMVECTOR(*) (const Mesh& mesh, const Submesh& submesh, uint32_t vertIndex);

    inline XMVECTOR getPosZkit(const zenkit::Mesh& mesh, const Unused& _, uint32_t vertIndex)
    {
        return toXM4Pos(mesh.vertices.at(mesh.polygons.vertex_indices.at(vertIndex)));
    }

    inline XMVECTOR getPosZlib(const ZenLoad::zCMesh& mesh, const Unused& _, uint32_t vertIndex)
    {
        return toXM4Pos(mesh.getVertices().at(mesh.getIndices().at(vertIndex)));
    }

    inline XMVECTOR getPosModelZkit(const zenkit::MultiResolutionMesh& mesh, const zenkit::SubMesh& submesh, uint32_t vertIndex)
    {
        uint32_t faceIndex = vertIndex / 3;
        uint32_t faceVertIndex = vertIndex % 3;
        const auto wedgeIndex = submesh.triangles.at(faceIndex).wedges[faceVertIndex];
        return toXM4Pos(mesh.positions.at(submesh.wedges.at(wedgeIndex).index));
    }

    inline XMVECTOR getPosModelZlib(const ZenLoad::zCProgMeshProto& mesh, const ZenLoad::zCProgMeshProto::SubMesh& submesh, uint32_t vertIndex)
    {
        uint32_t faceIndex = vertIndex / 3;
        uint32_t faceVertIndex = vertIndex % 3;
        auto wedgeIndex = submesh.m_TriangleList.at(faceIndex).m_Wedges[faceVertIndex];
        return toXM4Pos(mesh.getVertices().at(submesh.m_WedgeList.at(wedgeIndex).m_VertexIndex));
    }

    // POSITIONS - GET FACE POSITIONS

    template<typename Mesh, typename Submesh, GetPos<Mesh, Submesh> getPos, bool rescale, bool transform>
    array<XMVECTOR, 3> facePosToXm(
        const Mesh& mesh,
        const Submesh& submesh,
        uint32_t indexStart,
        const XMMATRIX& posTransform = identity)
    {
        array<XMVECTOR, 3> result;
        for (uint32_t i = 0; i < 3; i++) {
            result[i] = getPos(mesh, submesh, indexStart + i);
            if (rescale) {
                result[i] = XMVectorMultiply(result[i], XMVectorSet(0.01f, 0.01f, 0.01f, 1.f));
            }
            if (transform) {
                result[i] = XMVector4Transform(result[i], posTransform);
            }
        }
        return result;
    }

    // ###########################################################################
    // NORMALS
    // ###########################################################################
    
    // NORMALS - VECTOR ACCESSORS

    template <typename Mesh, typename Submesh>
    using GetNormal = XMVECTOR (*) (const Mesh& mesh, const Submesh& submesh, uint32_t vertIndex);

    inline XMVECTOR getNormalZkit(const zenkit::Mesh& mesh, const Unused& _, uint32_t vertIndex)
    {
        return toXM4Pos(mesh.features.at(mesh.polygon_feature_indices.at(vertIndex)).normal);
    }

    inline XMVECTOR getNormalZlib(const ZenLoad::zCMesh& mesh, const Unused& _, uint32_t vertIndex)
    {
        return toXM4Pos(mesh.getFeatures().at(mesh.getFeatureIndices().at(vertIndex)).vertNormal);
    }

    // TODO pass face index as well for models or everywhere or return entire face

    inline XMVECTOR getNormalModelZkit(const Unused& _, const zenkit::SubMesh& submesh, uint32_t vertIndex)
    {
        uint32_t faceIndex = vertIndex / 3;
        uint32_t faceVertIndex = vertIndex % 3;
        const auto wedgeIndex = submesh.triangles.at(faceIndex).wedges[faceVertIndex];
        return toXM4Pos(submesh.wedges.at(wedgeIndex).normal);
    }

    inline XMVECTOR getNormalModelZlib(const Unused& _, const ZenLoad::zCProgMeshProto::SubMesh& submesh, uint32_t vertIndex)
    {
        uint32_t faceIndex = vertIndex / 3;
        uint32_t faceVertIndex = vertIndex % 3;
        auto wedgeIndex = submesh.m_TriangleList.at(faceIndex).m_Wedges[faceVertIndex];
        return toXM4Pos(submesh.m_WedgeList.at(wedgeIndex).m_Normal);
    }

    // NORMALS - UTIL FUNCTIONS

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
            LOG(WARNING) << "Normals: " << util::leftPad("'" + std::to_string(normalStats.zero) + "/" + std::to_string(normalStats.total), 9)
                << "' are ZERO:  " << meshName;
        }
        if (normalStats.extreme > 0) {
            LOG(WARNING) << "Normals: " << util::leftPad("'" + std::to_string(normalStats.extreme) + "/" + std::to_string(normalStats.total), 9)
                << "' are WRONG: " << meshName << " (over 90 degrees off recalculated flat normal)";
        }
    }

    void transformDir(XMVECTOR& target, const XMVECTOR& source, const XMMATRIX& transform)
    {
        // XMVector3TransformNormal is hardcoded to use w=0, but might still output unnormalized normals when matrix has non-uniform scaling
        target = XMVector3TransformNormal(source, transform);
        target = XMVector3Normalize(target);
    }

    // NORMALS - GET FACE NORMALS

    // requires transform to only have position and/or rotation components, position is ignored
    template<typename Mesh, typename Submesh, GetNormal<Mesh, Submesh> getNormal, bool transform, bool debugChecksEnabled>
    std::pair<NormalsStats, array<XMVECTOR, 3>> faceNormalsToXm(
        const Mesh& mesh,
        const Submesh& submesh,
        uint32_t indexStart,
        const array<XMVECTOR, 3>& facePosXm,
        const XMMATRIX& normalTransform = identity)
    {
        NormalsStats normalStats = { .total = 3 };
        array<XMVECTOR, 3> result;

        XMVECTOR flatNormalXm;
        if (debugChecksEnabled) {
            flatNormalXm = calcFlatFaceNormal(facePosXm);
        }

        for (uint32_t i = 0; i < 3; i++) {
            const XMVECTOR normal = getNormal(mesh, submesh, indexStart + i);
            // TODO rewrite isZero to dxmath
            if (debugChecksEnabled && isZero(normal, zeroThreshold)) {
                result[i] = flatNormalXm;
                normalStats.zero++;
            }
            else {
                result[i] = normal;
                if (debugChecksEnabled) {
                    warnIfNotNormalized(result[i]);
                }
                if (transform) {
                    transformDir(result[i], result[i], normalTransform);
                }
            }

            if (debugChecksEnabled) {
                XMVECTOR angleXm = XMVector3AngleBetweenNormals(flatNormalXm, result[i]);
                float normalFlatnessRadian = XMVectorGetX(angleXm);
                float normalFlatnessDegrees = normalFlatnessRadian * (180.f / 3.141592653589793238463f);
                if (normalFlatnessDegrees > 90) {
                    normalStats.extreme++;
                }
            }
        }

        return { normalStats, result };
    }

    // ###########################################################################
    // UVs, LIGHT-COLOR
    // ###########################################################################

    inline UV getUvModelZkit(const Unused& _, const zenkit::SubMesh& submesh, uint32_t vertIndex)
    {
        uint32_t faceIndex = vertIndex / 3;
        uint32_t faceVertIndex = vertIndex % 3;
        const auto wedgeIndex = submesh.triangles.at(faceIndex).wedges[faceVertIndex];
        return toUv(submesh.wedges.at(wedgeIndex).texture);
    }

    inline UV getUvModelZlib(const Unused& _, const ZenLoad::zCProgMeshProto::SubMesh& submesh, uint32_t vertIndex)
    {
        uint32_t faceIndex = vertIndex / 3;
        uint32_t faceVertIndex = vertIndex % 3;
        auto wedgeIndex = submesh.m_TriangleList.at(faceIndex).m_Wedges[faceVertIndex];
        return toUv(submesh.m_WedgeList.at(wedgeIndex).m_Texcoord);
    }

    // ###########################################################################
    // LIGHTMAP UVs
    // ###########################################################################

    template <typename Mesh>
    using GetLightmapUvs = ARRAY_UV (*) (const Mesh&, uint32_t index);

    template <Vec3 Vec>
    UV calculateLightmapUvs(Vec lmOrigin, Vec lmNormalUp, Vec lmNormalRight, XMVECTOR posXm)
    {
        XMVECTOR rescale = toXM4Pos(VEC3{ 100, 100, 100 });
        XMVECTOR lmDir = XMVectorMultiply(posXm, rescale) - toXM4Pos(lmOrigin);
        float u = XMVectorGetX(XMVector3Dot(lmDir, toXM4Dir(lmNormalRight)));
        float v = XMVectorGetX(XMVector3Dot(lmDir, toXM4Dir(lmNormalUp)));
        return { u, v };
    }

    ARRAY_UV getLightmapUvsZkit(const zenkit::Mesh& mesh, uint32_t faceIndex, XMVECTOR posXm)
    {
        int16_t lightmapIndex = mesh.polygons.lightmap_indices.at(faceIndex);
        if (lightmapIndex != -1) {
            zenkit::LightMap lightmap = mesh.lightmaps.at(lightmapIndex);
            auto [u, v] = calculateLightmapUvs(lightmap.origin, lightmap.normals[0], lightmap.normals[1], posXm).vec;
            float i = lightmap.texture_index;
            return { u, v, i };
        }
        else {
            return { 0, 0, -1 };
        }
    }

    ARRAY_UV getLightmapUvsZlib(const ZenLoad::zCMesh& mesh, uint32_t faceIndex, XMVECTOR posXm)
    {
        int16_t lightmapIndex = mesh.getTriangleLightmapIndices().at(faceIndex);
        if (lightmapIndex != -1) {
            ZenLoad::Lightmap lightmap = mesh.getLightmapReferences().at(lightmapIndex);
            auto [u, v] = calculateLightmapUvs(lightmap.origin, lightmap.normalUp, lightmap.normalRight, posXm).vec;
            float i = (float)lightmap.lightmapTextureIndex;
            return { u, v, i };
        }
        else {
            return { 0, 0, -1 };
        }
    }

    // ###########################################################################
    // UTIL FUNCTIONS
    // ###########################################################################

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

    bool isMeshEmptyAndValidateZkit(const zenkit::Mesh& mesh, bool expectLigtmaps)
    {
        if (mesh.polygons.vertex_indices.empty()) {
            return true;
        }
        if (expectLigtmaps && mesh.polygons.lightmap_indices.empty()) {
            ::util::throwError("Expected world mesh to have lightmap information!");
        }
        if (!expectLigtmaps && !mesh.polygons.lightmap_indices.empty()) {
            ::util::throwError("Expected VOB mesh to NOT have lightmap information!");
        }
        return false;
    }

    bool isMeshEmptyAndValidate(const ZenLoad::zCMesh& mesh, bool expectLigtmaps)
    {
        if (mesh.getIndices().empty()) {
            return true;
        }
        if (expectLigtmaps && mesh.getTriangleLightmapIndices().empty()) {
            ::util::throwError("Expected world mesh to have lightmap information!");
        }
        if (!expectLigtmaps && !mesh.getTriangleLightmapIndices().empty()) {
            ::util::throwError("Expected VOB mesh to NOT have lightmap information!");
        }
        return false;
    }

    bool isSubmeshEmptyAndValidate(const ZenLoad::zCProgMeshProto::SubMesh& submesh)
    {
        if (submesh.m_TriangleList.empty()) {
            return true;
        }
        return false;
    }

    static_assert(std::is_enum_v<zenkit::AlphaFunction>);

    template<typename T>
    bool logIfPropertyUnusual(const string& objName, const string& propName, T expected, T actual)
    {
        if (expected != actual) {
            if constexpr (std::is_enum_v<T>) {
                LOG(INFO) << "Unusual property '" << propName << "' = '" << magic_enum::enum_name(actual) << "' on " << objName;
            }
            else {
                LOG(INFO) << "Unusual property '" << propName << "' = '"<< actual << "' on " << objName;
            }
            return true;
        }
        return false;
    }

    bool checkForUnusualMatProperties(const zenkit::Material material)
    {
        string name = material.texture;
        bool unusual = false;
        unusual |= logIfPropertyUnusual(name, "alpha_func", zenkit::AlphaFunction::DEFAULT, material.alpha_func);
        unusual |= logIfPropertyUnusual(name, "force_occluder", false, material.force_occluder);// TODO find occluders if they exist
        // disable_collision is for waterfalls
        return unusual;
    }

    // ###########################################################################
    // LOAD WORLD
    // ###########################################################################

    void loadWorldMeshZkit(
        VERT_CHUNKS_BY_MAT& target,
        const zenkit::Mesh& worldMesh,
        bool debugChecksEnabled)
    {
        if (isMeshEmptyAndValidateZkit(worldMesh, true)) {
            ::util::throwError("World mesh is empty!");
        }
        NormalsStats normalStats;
        uint32_t faceCount = worldMesh.polygons.material_indices.size();

        for (uint32_t faceIndex = 0, vertIndex = 0; faceIndex < faceCount;) {

            uint32_t meshMatIndex = worldMesh.polygons.material_indices.at(faceIndex);
            zenkit::Material meshMat = worldMesh.materials.at(meshMatIndex);
            bool unusual = checkForUnusualMatProperties(meshMat);

            const Material material = { getTexId(::util::asciiToLower(meshMat.texture)) };
            unordered_map<ChunkIndex, VEC_VERTEX_DATA>& materialData = ::util::getOrCreateDefault(target, material);

            uint32_t nextMeshMatIndex;
            do {
                // positions
                auto facePosXm = facePosToXm<zenkit::Mesh, Unused, getPosZkit, true, false>(worldMesh, {}, vertIndex);

                // normals
                array<XMVECTOR, 3> faceNormalsXm;
                NormalsStats faceNormalStats;
                if (debugChecksEnabled) {
                    std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<zenkit::Mesh, Unused, getNormalZkit, false, true>(
                        worldMesh, {}, vertIndex, facePosXm);
                }
                else {
                    std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<zenkit::Mesh, Unused, getNormalZkit, false, false>(
                        worldMesh, {}, vertIndex, facePosXm);
                }
                normalStats += faceNormalStats;

                // other
                array<VERTEX_POS, 3> facePos;
                array<VERTEX_OTHER, 3> faceOther;

                for (uint32_t i = 0; i < 3; i++) {
                    const auto& feature = worldMesh.features.at(worldMesh.polygons.feature_indices.at(vertIndex));

                    VERTEX_POS pos = toVec3(facePosXm[i]);
                    VERTEX_OTHER other;
                    other.normal = toVec3(faceNormalsXm[i]);
                    const auto& glmUvDiffuse = feature.texture;
                    other.uvDiffuse = toUv(glmUvDiffuse);
                    other.colLight = fromSRGB(COLOR(feature.light));
                    other.dirLight = { -100.f, -100.f, -100.f };// value that is easy to check as not normalized in shader
                    other.lightSun = 1.0f;
                    other.uvLightmap = getLightmapUvsZkit(worldMesh, faceIndex, facePosXm[i]);

                    facePos.at(2 - i) = pos;
                    faceOther.at(2 - i) = other;
                    vertIndex++;
                }

                const ChunkIndex chunkIndex = toChunkIndex(centroidPos(facePosXm));
                VEC_VERTEX_DATA& chunkData = ::util::getOrCreateDefault(materialData, chunkIndex);
                insertFace(chunkData, facePos, faceOther);

                faceIndex++;
                if (faceIndex >= faceCount) {
                    break;
                }
                nextMeshMatIndex = worldMesh.polygons.material_indices.at(faceIndex);
            }
            while (nextMeshMatIndex == meshMatIndex);
        }

        if (debugChecksEnabled) {
            logNormalStats(normalStats, "LEVEL_MESH");
        }
    }

    void loadWorldMeshActual(
        VERT_CHUNKS_BY_MAT& target,
        const ZenLoad::zCMesh* worldMesh,
        bool debugChecksEnabled)
    {
        if (isMeshEmptyAndValidate(*worldMesh, true)) {
            ::util::throwError("World mesh is empty!");
        }
        NormalsStats normalStats;
        
        uint32_t vertCount = worldMesh->getIndices().size();
        uint32_t faceCount = vertCount / 3;

        for (uint32_t faceIndex = 0, vertIndex = 0; faceIndex < faceCount;) {

            uint32_t meshMatIndex = worldMesh->getTriangleMaterialIndices().at(faceIndex);
            ZenLoad::zCMaterialData meshMat = worldMesh->getMaterials().at(meshMatIndex);

            const Material material = { getTexId(::util::asciiToLower(meshMat.texture)) };
            unordered_map<ChunkIndex, VEC_VERTEX_DATA>& materialData = ::util::getOrCreateDefault(target, material);

            uint32_t nextMeshMatIndex;
            do {
                // positions
                auto facePosXm = facePosToXm<ZenLoad::zCMesh, Unused, getPosZlib, true, false>(*worldMesh, {}, vertIndex);

                // normals
                array<XMVECTOR, 3> faceNormalsXm;
                NormalsStats faceNormalStats;
                if (debugChecksEnabled) {
                    std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<ZenLoad::zCMesh, Unused, getNormalZlib, false, true>(
                        *worldMesh, {}, vertIndex, facePosXm);
                }
                else {
                    std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<ZenLoad::zCMesh, Unused, getNormalZlib, false, false>(
                        *worldMesh, {}, vertIndex, facePosXm);
                }
                normalStats += faceNormalStats;

                // other
                array<VERTEX_POS, 3> facePos;
                array<VERTEX_OTHER, 3> faceOther;

                for (uint32_t i = 0; i < 3; i++) {
                    const auto& feature = worldMesh->getFeatures().at(worldMesh->getFeatureIndices().at(vertIndex));

                    VERTEX_POS pos = toVec3(facePosXm[i]);
                    VERTEX_OTHER other;
                    other.normal = toVec3(faceNormalsXm[i]);
                    other.uvDiffuse = { feature.uv[0], feature.uv[1] };
                    other.colLight = fromSRGB(COLOR(feature.lightStat));
                    other.dirLight = { -100.f, -100.f, -100.f };// value that is easy to check as not normalized in shader
                    other.lightSun = 1.0f;
                    other.uvLightmap = getLightmapUvsZlib(*worldMesh, faceIndex, facePosXm[i]);

                    facePos.at(2 - i) = pos;
                    faceOther.at(2 - i) = other;
                    vertIndex++;
                }

                const ChunkIndex chunkIndex = toChunkIndex(centroidPos(facePosXm));
                VEC_VERTEX_DATA& chunkData = ::util::getOrCreateDefault(materialData, chunkIndex);
                insertFace(chunkData, facePos, faceOther);

                faceIndex++;
                if (faceIndex >= faceCount) {
                    break;
                }
                nextMeshMatIndex = worldMesh->getTriangleMaterialIndices().at(faceIndex);
            } while (nextMeshMatIndex == meshMatIndex);
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
            loadWorldMeshActual(target, worldMesh, debugChecksEnabled);
        }
    }

    // ###########################################################################
    // LOAD VOB
    // ###########################################################################

    XMMATRIX inversedTransposed(const XMMATRIX& source) {
        const XMMATRIX transposed = XMMatrixTranspose(source);
        return XMMatrixInverse(nullptr, transposed);
    }

    unordered_set<string> processedVisuals;

    void loadInstanceMeshZkit(
        VERT_CHUNKS_BY_MAT& target,
        const zenkit::MultiResolutionMesh& mesh,
        const StaticInstance& instance,
        bool debugChecksEnabled)
    {
        using Mesh = zenkit::MultiResolutionMesh;
        using Submesh = zenkit::SubMesh;

        NormalsStats normalStats;
        
        if (mesh.sub_meshes.size() == 0) {
            LOG(WARNING) << "Model contained no geometry! " << instance.meshName;
            return;
        }

        for (const auto& submesh : mesh.sub_meshes) {

            // TODO check empty geometry?

            zenkit::Material meshMat = submesh.mat;
            bool unusual = checkForUnusualMatProperties(meshMat);

            if (meshMat.texture.empty()) {
                LOG(WARNING) << "Skipped VOB submesh because of empty texture! " << instance.meshName;
                continue;
            }

            const Material material = { getTexId(::util::asciiToLower(meshMat.texture)) };
            unordered_map<ChunkIndex, VEC_VERTEX_DATA>& materialData = ::util::getOrCreateDefault(target, material);

            uint32_t faceCount = submesh.triangles.size();

            for (uint32_t faceIndex = 0, vertIndex = 0; faceIndex < faceCount; faceIndex++) {
                // positions
                array<XMVECTOR, 3> facePosXm = facePosToXm<Mesh, Submesh, getPosModelZkit, true, true>(
                    mesh, submesh, vertIndex, instance.transform);

                // normals
                array<XMVECTOR, 3> faceNormalsXm;
                NormalsStats faceNormalStats;
                if (debugChecksEnabled) {
                    std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<Unused, Submesh, getNormalModelZkit, true, true>(
                        {}, submesh, vertIndex, facePosXm, instance.transform);
                }
                else {
                    std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<Unused, Submesh, getNormalModelZkit, true, false>(
                        {}, submesh, vertIndex, facePosXm, instance.transform);
                }
                normalStats += faceNormalStats;

                // other
                array<VERTEX_POS, 3> facePos;
                array<VERTEX_OTHER, 3> faceOther;

                for (uint32_t i = 0; i < 3; i++) {
                    VERTEX_POS pos = toVec3(facePosXm[i]);
                    VERTEX_OTHER other;
                    other.normal = toVec3(faceNormalsXm[i]);
                    other.uvDiffuse = getUvModelZkit({}, submesh, vertIndex);
                    other.uvLightmap = { 0, 0, -1 };
                    other.colLight = instance.lighting.color;
                    other.dirLight = toVec3(XMVector3Normalize(instance.lighting.direction));
                    other.lightSun = instance.lighting.receiveLightSun ? 1.f : 0.f;

                    // flip faces (seems like zEngine uses counter-clockwise winding, while we use clockwise winding)
                    // TODO use D3D11_RASTERIZER_DESC FrontCounterClockwise instead?
                    facePos.at(2 - i) = pos;
                    faceOther.at(2 - i) = other;
                    vertIndex++;
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

    void loadInstanceMesh(
        VERT_CHUNKS_BY_MAT& target,
        const ZenLoad::zCProgMeshProto& mesh,
        const StaticInstance& instance,
        bool debugChecksEnabled)
    {
        NormalsStats normalStats;
        if (mesh.getNumSubmeshes() == 0) {
            LOG(WARNING) << "Model contained no geometry! " << instance.meshName;
            return;
        }
        using Submesh = ZenLoad::zCProgMeshProto::SubMesh;

        for (uint32_t i = 0; i < mesh.getNumSubmeshes(); i++) {
            auto& submesh = mesh.getSubmesh(i);
            if (isSubmeshEmptyAndValidate(submesh)) {
                LOG(WARNING) << "Skipped VOB submesh because of empty geometry! " << instance.meshName;
                continue;
            }

            ZenLoad::zCMaterialData meshMat = submesh.m_Material;
            if (meshMat.texture.empty()) {
                LOG(WARNING) << "Skipped VOB submesh because of empty texture! " << instance.meshName;
                continue;
            }

            const Material material = { getTexId(::util::asciiToLower(meshMat.texture)) };
            unordered_map<ChunkIndex, VEC_VERTEX_DATA>& materialData = ::util::getOrCreateDefault(target, material);

            uint32_t faceCount = submesh.m_TriangleList.size();

            for (uint32_t faceIndex = 0, vertIndex = 0; faceIndex < faceCount; faceIndex++) {

                // positions
                array<XMVECTOR, 3> facePosXm = facePosToXm<ZenLoad::zCProgMeshProto, Submesh, getPosModelZlib, true, true>(
                                mesh, submesh, vertIndex, instance.transform);

                // normals
                array<XMVECTOR, 3> faceNormalsXm;
                NormalsStats faceNormalStats;
                if (debugChecksEnabled) {
                    std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<Unused, Submesh, getNormalModelZlib, true, true>(
                        {}, submesh, vertIndex, facePosXm, instance.transform);
                }
                else {
                    std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<Unused, Submesh, getNormalModelZlib, true, false>(
                        {}, submesh, vertIndex, facePosXm, instance.transform);
                }
                normalStats += faceNormalStats;

                // other
                array<VERTEX_POS, 3> facePos;
                array<VERTEX_OTHER, 3> faceOther;

                for (uint32_t i = 0; i < 3; i++) {
                    //auto wedgeIndex = submesh.m_TriangleList.at(faceIndex).m_Wedges[i];
                    //const auto& vertFeature = submesh.m_WedgeList.at(wedgeIndex);

                    VERTEX_POS pos = toVec3(facePosXm[i]);
                    VERTEX_OTHER other;
                    other.normal = toVec3(faceNormalsXm[i]);
                    other.uvDiffuse = getUvModelZlib({}, submesh, vertIndex);
                    other.uvLightmap = { 0, 0, -1 };
                    other.colLight = instance.lighting.color;
                    other.dirLight = toVec3(XMVector3Normalize(instance.lighting.direction));
                    other.lightSun = instance.lighting.receiveLightSun ? 1.f : 0.f;

                    // flip faces (seems like zEngine uses counter-clockwise winding, while we use clockwise winding)
                    // TODO use D3D11_RASTERIZER_DESC FrontCounterClockwise instead?
                    facePos.at(2 - i) = pos;
                    faceOther.at(2 - i) = other;
                    vertIndex++;
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

    XMMATRIX rescale(const XMMATRIX& transform)
    {
        // "rescaling" means scaling the translation part of the matrix, not actually applying a scale transform
        XMVECTOR scaleXm, rotQuatXm, transXm;
        bool success = XMMatrixDecompose(&scaleXm, &rotQuatXm, &transXm, transform);
        assert(success);
        return XMMatrixAffineTransformation(scaleXm, g_XMZero, rotQuatXm, transXm * G_ASSET_RESCALE);
    }

    void loadInstanceModelZkit(
        VERT_CHUNKS_BY_MAT& target,
        const zenkit::ModelHierarchy& hierarchy,
        const zenkit::ModelMesh& model,
        const StaticInstance& instance,
        bool debugChecksEnabled)
    {
        // It's unclear why negation is needed and if only z component or other components needs to be negated as well.
        // If z is not negated, shisha's in Gomez' throneroom (G1) are not placed correctly.
        XMMATRIX transformRoot = XMMatrixTranslationFromVector(toXM4Dir(hierarchy.root_translation) * -1 * G_ASSET_RESCALE);
        for (const auto& mesh : model.meshes) {
            // TODO softskin animation
            StaticInstance newInstance = instance;
            newInstance.transform = XMMatrixMultiply(transformRoot, instance.transform);
            loadInstanceMeshZkit(target, mesh.mesh, newInstance, debugChecksEnabled);
        }

        unordered_map<string, uint32_t> attachmentToNode;
        for (uint32_t i = 0; i < hierarchy.nodes.size(); i++) {
            const auto& node = hierarchy.nodes.at(i);
            attachmentToNode.insert({ node.name, i });
        }

        for (const auto& [name, mesh] : model.attachments) {
            auto index = attachmentToNode.at(name);
            auto node = &hierarchy.nodes.at(index);

            XMMATRIX transform = rescale(toXMMatrix(glm::value_ptr(node->transform)));
            while (node->parent_index >= 0) {
                const auto& parent = hierarchy.nodes.at(node->parent_index);
                XMMATRIX transformParent = rescale(toXMMatrix(glm::value_ptr(parent.transform)));
                transform = XMMatrixMultiply(transform, transformParent);
                node = &parent;
            }
            // rootNode translation is not applied to attachments (example: see kettledrum placement on oldcamp music stage)

            StaticInstance newInstance = instance;
            newInstance.transform = XMMatrixMultiply(transform, instance.transform);
            loadInstanceMeshZkit(target, mesh, newInstance, debugChecksEnabled);
        }
    }

    void loadInstanceMeshLib(
        VERT_CHUNKS_BY_MAT& target,
        const ZenLoad::zCModelMeshLib& lib,
        const StaticInstance& instance,
        bool debugChecksEnabled)
    {
        for (auto mesh : lib.getMeshes()) {
            auto trans = lib.getRootNodeTranslation();
            // It's unclear why negation is needed and if only z component or other components needs to be negated as well.
            // If z is not negated, shisha's in Gomez' throneroom (G1) are not placed correctly.
            XMMATRIX transformRoot = XMMatrixTranslationFromVector(XMVectorSet(-trans.x, -trans.y, -trans.z, 0));

            StaticInstance newInstance = instance;
            newInstance.transform = XMMatrixMultiply(transformRoot, instance.transform);
            loadInstanceMesh(target, mesh.getMesh(), newInstance, debugChecksEnabled);
        }

        for (const auto& [name, mesh] : lib.getAttachments()) {
            auto index = lib.findNodeIndex(name);

            auto node = lib.getNodes()[index];
            XMMATRIX transform = toXMMatrix(node.transformLocal.mv);

            while (node.parentValid()) {
                const auto& parent = lib.getNodes()[node.parentIndex];
                XMMATRIX transformParent = toXMMatrix(parent.transformLocal.mv);
                transform = XMMatrixMultiply(transform, transformParent);
                node = parent;
            }
            // rootNode translation is not applied to attachments (example: see kettledrum placement on oldcamp music stage)

            StaticInstance newInstance = instance;
            newInstance.transform = XMMatrixMultiply(transform, instance.transform);
            loadInstanceMesh(target, mesh, newInstance, debugChecksEnabled);
        }
    }
}