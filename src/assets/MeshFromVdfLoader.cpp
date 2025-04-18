#include "stdafx.h"
#include "MeshFromVdfLoader.h"

#include <type_traits>
#include <filesystem>
#include <variant>

#include <cstring>

#include "AssetCache.h"
#include "render/MeshUtil.h"
#include "../Util.h"

#include "../lib/meshoptimizer/src/meshoptimizer.h"// TODO wtf??
#include "magic_enum.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace assets
{
    using namespace render;
	using namespace DirectX;
    using ::std::string;
    using ::std::array;
    using ::std::vector;
    using ::std::unordered_map;
    using ::std::unordered_set;
    using ::std::optional;
	using ::util::getOrCreate;

    constexpr bool meshoptOptimize = true;
    constexpr bool manualReservation = false;

    const float zeroThreshold = 0.000001f;
    const XMMATRIX identity = XMMatrixIdentity();

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

    struct LoadStats {
        NormalsStats normalsWorldMesh;
        std::unordered_map<std::string, NormalsStats> normalsInstances;
        std::unordered_map<std::string, bool> skippedNoTexSubmeshInstances;
        std::unordered_map<zenkit::AlphaFunction, uint32_t> materialAlphas;
        std::unordered_map<zenkit::MaterialGroup, uint32_t> materialGroups;
    } loadStats;

    typedef std::monostate Unused;

    // Either of type T if conditional is true, or of type std::monostate (which is essentially usable void type)
    template<bool TEST, typename T>
    using OPT_PARAM = std::conditional<TEST, T, Unused>::type;

    static_assert(XYZ<glm::vec3>);
    static_assert(XY<glm::vec2>);

    template<typename MESH, typename SUBMESH>
    concept IS_WORLD_MESH = std::is_same_v<MESH, zenkit::Mesh>;

    template<typename MESH, typename SUBMESH>
    concept IS_MODEL_MESH = std::is_same_v<MESH, zenkit::MultiResolutionMesh> && std::is_same_v<SUBMESH, zenkit::SubMesh>;

    struct Face {
        array<VertexPos, 3> pos;
        array<VertexBasic, 3> features;
        ChunkIndex chunkIndex;
    };

    // TODO Prepare for index buffer creation:
    // We should really at some point switch to processing/converting all vertex positions/features first
    // and THEN validating correct values for each face. Current approach is not really compatible with index buffers.

    // ###########################################################################
    // POSITIONS
    // ###########################################################################

    inline XMVECTOR getPosWorldZkit(const zenkit::Mesh& mesh, uint32_t vertIndex)
    {
        return toXM4Pos(mesh.vertices.at(mesh.polygons.vertex_indices.at(vertIndex)));
    }

    inline XMVECTOR getPosModelZkit(const zenkit::MultiResolutionMesh& mesh, const zenkit::SubMesh& submesh, uint32_t faceIndex, uint32_t faceVertIndex)
    {
        const auto wedgeIndex = submesh.triangles.at(faceIndex).wedges[faceVertIndex];
        return toXM4Pos(mesh.positions.at(submesh.wedges.at(wedgeIndex).index));
    }

    template<typename MESH, typename SUBMESH, bool rescale, bool transform>
    array<XMVECTOR, 3> facePosToXm(
        const MESH& mesh,
        const SUBMESH& submesh,
        uint32_t faceIndex,
        uint32_t vertIndex,
        const XMMATRIX& posTransform = identity)
    {
        array<XMVECTOR, 3> result;
        for (uint32_t i = 0; i < 3; i++) {
            if constexpr (IS_WORLD_MESH<MESH, SUBMESH>) {
                result[i] = getPosWorldZkit(mesh, vertIndex + i);
            }
            else if constexpr (IS_MODEL_MESH<MESH, SUBMESH>) {
                result[i] = getPosModelZkit(mesh, submesh, faceIndex, i);
            }
            else {
                static_assert(false);
            }
            if constexpr (rescale) {
                result[i] = XMVectorMultiply(result[i], XMVectorSet(G_ASSET_RESCALE, G_ASSET_RESCALE, G_ASSET_RESCALE, 1.f));
            }
            if constexpr (transform) {
                result[i] = XMVector4Transform(result[i], posTransform);
            }
        }
        return result;
    }

    // ###########################################################################
    // NORMALS
    // ###########################################################################
    
    // NORMALS - VECTOR ACCESSORS

    template <typename MESH, typename SUBMESH>
    using GetNormal = XMVECTOR (*) (const MESH& mesh, const SUBMESH& submesh, uint32_t vertIndex);

    inline XMVECTOR getNormalZkit(const zenkit::Mesh& mesh, const Unused& _, uint32_t vertIndex)
    {
        return toXM4Pos(mesh.features.at(mesh.polygons.feature_indices.at(vertIndex)).normal);
    }

    // TODO pass face index as well for models or everywhere or return entire face

    inline XMVECTOR getNormalModelZkit(const Unused& _, const zenkit::SubMesh& submesh, uint32_t vertIndex)
    {
        uint32_t faceIndex = vertIndex / 3;
        uint32_t faceVertIndex = vertIndex % 3;
        const auto wedgeIndex = submesh.triangles.at(faceIndex).wedges[faceVertIndex];
        return toXM4Pos(submesh.wedges.at(wedgeIndex).normal);
    }

    // NORMALS - UTIL FUNCTIONS

    void logNormalStats(const NormalsStats& normalStats, const std::string& meshName)
    {
        if (normalStats.zero > 0) {
            LOG(WARNING) << "    Normals: " << util::leftPad("'" + std::to_string(normalStats.zero) + "/" + std::to_string(normalStats.total), 9)
                << "' are zero:  " << meshName;
        }
        if (normalStats.extreme > 0) {
            LOG(WARNING) << "    Normals: " << util::leftPad("'" + std::to_string(normalStats.extreme) + "/" + std::to_string(normalStats.total), 9)
                << "' are wrong: " << meshName << " (over 90 degrees off recalculated flat normal)";
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
    template<typename MESH, typename SUBMESH, GetNormal<MESH, SUBMESH> getNormal, bool transform, bool debugChecksEnabled>
    std::pair<NormalsStats, array<XMVECTOR, 3>> faceNormalsToXm(
        const MESH& mesh,
        const SUBMESH& submesh,
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

    inline Uv getUvModelZkit(const Unused& _, const zenkit::SubMesh& submesh, uint32_t vertIndex)
    {
        uint32_t faceIndex = vertIndex / 3;
        uint32_t faceVertIndex = vertIndex % 3;
        const auto wedgeIndex = submesh.triangles.at(faceIndex).wedges[faceVertIndex];
        return toUv(submesh.wedges.at(wedgeIndex).texture);
    }

    // ###########################################################################
    // LIGHTMAP UVs
    // ###########################################################################

    template <typename MESH>
    using GetLightmapUvs = Uvi (*) (const MESH&, uint32_t index);

    template <XYZ V3>
    Uv calculateLightmapUvs(V3 lmOrigin, V3 lmNormalUp, V3 lmNormalRight, XMVECTOR posXm)
    {
        XMVECTOR rescale = toXM4Pos(Vec3{ 100, 100, 100 });
        XMVECTOR lmDir = XMVectorMultiply(posXm, rescale) - toXM4Pos(lmOrigin);
        float u = XMVectorGetX(XMVector3Dot(lmDir, toXM4Dir(lmNormalRight)));
        float v = XMVectorGetX(XMVector3Dot(lmDir, toXM4Dir(lmNormalUp)));
        return { u, v };
    }

    Uvi getLightmapUvsZkit(const zenkit::Mesh& mesh, uint32_t faceIndex, XMVECTOR posXm)
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

    // ###########################################################################
    // UTIL FUNCTIONS
    // ###########################################################################

    template <VERTEX_FEATURE F>
    void insertFace(Verts<F>& target, const array<VertexPos, 3>& facePos, const array<F, 3>& faceOther) {
        // manual reservation strategy helps with big vectors
        // TODO find out what our actual memory problem is, we should be able to allocate 3.2G!! does DX11 driver take a lot?
        if constexpr (manualReservation) {
            uint32_t vertReserveCount = 200 * 3;
            size_t blockSize = vertReserveCount;
            size_t currentSize = target.vecPos.size();
            if (currentSize == 0) {
                target.vecPos.reserve(vertReserveCount);
                target.vecOther.reserve(vertReserveCount);
            }
            else if (currentSize >= (vertReserveCount * 128)) {
                blockSize = vertReserveCount * 128;
            }
            else if (currentSize >= (vertReserveCount * 12)) {
                blockSize = vertReserveCount * 48;
            }
            else if (currentSize >= (vertReserveCount * 4)) {
                blockSize = vertReserveCount * 12;
            }
            else if (currentSize >= (vertReserveCount * 1)) {
                blockSize = vertReserveCount * 4;
            }
            if (currentSize != 0 && currentSize % vertReserveCount == 0) {
                uint32_t blocks = (currentSize / blockSize) + 1;
                target.vecPos.reserve(blocks * blockSize);
                target.vecOther.reserve(blocks * blockSize);
            }
        }
        target.vecPos.push_back(facePos[0]);
        target.vecPos.push_back(facePos[1]);
        target.vecPos.push_back(facePos[2]);
        target.vecOther.push_back(faceOther[0]);
        target.vecOther.push_back(faceOther[1]);
        target.vecOther.push_back(faceOther[2]);
    }

    array<Vec3, 2> createVertlistBbox(const vector<VertexPos>& verts)
    {
        BoundingBox bbox;
        BoundingBox::CreateFromPoints(bbox, verts.size(), (XMFLOAT3*) verts.data(), sizeof(Vec3));
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
        //unusual |= logIfPropertyUnusual(name, "alpha_func", zenkit::AlphaFunction::DEFAULT, material.alpha_func);
        unusual |= logIfPropertyUnusual(name, "force_occluder", false, material.force_occluder);// TODO find occluders if they exist
        // disable_collision is for waterfalls
        return unusual;
    }

    optional<BlendType> getBlendType(zenkit::AlphaFunction alphaFunc)
    {
        switch (alphaFunc) {
        case zenkit::AlphaFunction::DEFAULT:
        case zenkit::AlphaFunction::NONE:
            return BlendType::NONE;
        case zenkit::AlphaFunction::BLEND:
            // TODO is actual alpha channel blending even a thing?
            return BlendType::BLEND_FACTOR;
        case zenkit::AlphaFunction::ADD:
            return BlendType::ADD;
        case zenkit::AlphaFunction::MULTIPLY_ALT:
            return BlendType::MULTIPLY;
        default:
            LOG(WARNING) << "Skipping material due to unsupported blend type! " << magic_enum::enum_name(alphaFunc);
            return std::nullopt;
        }
    }

    optional<BlendType> getBlendType(zenkit::Material material)
    {
        auto result = getBlendType(material.alpha_func);
        if (result.has_value() && result.value() == BlendType::NONE && material.group == zenkit::MaterialGroup::WATER) {
            // TODO is this a G1 only thing or does G2 also want this?
            return BlendType::BLEND_FACTOR;
        }
        return result;
    }

    optional<Material> createMaterial(zenkit::Material material, bool debugChecksEnabled)
    {
        bool unusual = checkForUnusualMatProperties(material);

        if (debugChecksEnabled) {
            util::getOrCreateDefault(loadStats.materialGroups, material.group)++;
            util::getOrCreateDefault(loadStats.materialAlphas, material.alpha_func)++;
        }

        auto blendTypeOpt = getBlendType(material);
        if (!blendTypeOpt.has_value()) {
            return std::nullopt;
        }
        BlendType blendType = blendTypeOpt.value();
        bool isLinear = blendType == BlendType::MULTIPLY;

        return Material {
            .texBaseColor = getTexId(util::asciiToLower(material.texture)),
            .blendType = blendType,
            .colorSpace = isLinear ? ColorSpace::LINEAR : ColorSpace::SRGB,
        };
    }

    optional<Material> createMaterialDecal(const std::string textureName, const Decal& decal, bool debugChecksEnabled)
    {
        if (debugChecksEnabled) {
            util::getOrCreateDefault(loadStats.materialAlphas, decal.alpha)++;
        }

        auto blendTypeOpt = getBlendType(decal.alpha);
        if (!blendTypeOpt.has_value()) {
            return std::nullopt;
        }
        BlendType blendType = blendTypeOpt.value();
        bool isLinear = blendType == BlendType::MULTIPLY;

        return Material {
            .texBaseColor = getTexId(util::asciiToLower(textureName)),
            .blendType = blendType,
            .colorSpace = isLinear ? ColorSpace::LINEAR : ColorSpace::SRGB,
        };
    }

    // ###########################################################################
    // PACKING
    // ###########################################################################

    /**
     * Used to either generate entirely new indices, resulting in reduced vertex feature buffer sizes.
     * Or to optimize existing buffers, resulting in unchanged index and vertex feature buffer sizes.
     */
    template <VERTEX_FEATURE F>
    void meshoptApplyRemap(Verts<F>& target, vector<unsigned int> remap, uint32_t newIndexCount, uint32_t previousVertCount, uint32_t newVertCount)
    {
        const unsigned int* indicesDataOrNullPtr = target.vecIndex.empty() ? nullptr : target.vecIndex.data();
        if (target.vecIndex.empty()) {
            target.vecIndex.resize(newIndexCount);
        }
        meshopt_remapIndexBuffer(target.vecIndex.data(), indicesDataOrNullPtr, newIndexCount, remap.data());
        meshopt_remapVertexBuffer(target.vecPos.data(), target.vecPos.data(), previousVertCount, sizeof(VertexPos), remap.data());
        meshopt_remapVertexBuffer(target.vecOther.data(), target.vecOther.data(), previousVertCount, sizeof(F), remap.data());
        if (newVertCount != previousVertCount) {
            assert(newVertCount < previousVertCount);
            target.vecPos.resize(newVertCount);
            target.vecOther.resize(newVertCount);
        }
    }

    template <VERTEX_FEATURE F>
    uint32_t createIndicesAndRemap(Verts<F>& verts, uint32_t indexCount)
    {
        // assert that we are dealing with entirely unindex vertex data
        assert(verts.vecIndex.empty());
        assert(verts.vecPos.size() == indexCount);
        assert(verts.vecOther.size() == indexCount);

        array streams = {
            meshopt_Stream {verts.vecPos.data(), sizeof(VertexPos), sizeof(VertexPos)},
            meshopt_Stream {verts.vecOther.data(), sizeof(VertexBasic), sizeof(VertexBasic)},
        };
        std::vector<unsigned int> remap(indexCount); // temporary remap table
        uint32_t vertexCount = meshopt_generateVertexRemapMulti(remap.data(), nullptr, indexCount, indexCount, streams.data(), streams.size());
        meshoptApplyRemap(verts, remap, indexCount, indexCount, vertexCount);

        return vertexCount;
    }

    template <VERTEX_FEATURE F>
    void optimizeIndicesAndVerts(Verts<F>& verts, uint32_t indexCount, uint32_t vertexCount)
    {
        assert(verts.vecIndex.size() == indexCount);
        assert(verts.vecPos.size() == vertexCount);
        assert(verts.vecOther.size() == vertexCount);

        meshopt_optimizeVertexCache(verts.vecIndex.data(), verts.vecIndex.data(), indexCount, vertexCount);

        const float* vertPosStart = verts.vecPos.data()->vec;
        meshopt_optimizeOverdraw(verts.vecIndex.data(), verts.vecIndex.data(), indexCount, vertPosStart, vertexCount, sizeof(VertexPos), 1.05f);

        std::vector<unsigned int> remap(vertexCount); // temporary remap table
        uint32_t remapSize = meshopt_optimizeVertexFetchRemap(remap.data(), verts.vecIndex.data(), indexCount, vertexCount);
        assert(remapSize == vertexCount);
        meshoptApplyRemap(verts, remap, indexCount, vertexCount, vertexCount);
    }

    // ###########################################################################
    // LOAD WORLD
    // ###########################################################################

    Face loadWorldFace(
        const zenkit::Mesh& mesh,
        uint32_t faceIndex,
        uint32_t vertIndex,
        bool debugChecksEnabled,
        NormalsStats& normalStats)
    {
        // positions
        auto facePosXm = facePosToXm<zenkit::Mesh, Unused, true, false>(mesh, {}, faceIndex, vertIndex);

        // normals
        array<XMVECTOR, 3> faceNormalsXm;
        NormalsStats faceNormalStats;
        if (debugChecksEnabled) {
            std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<zenkit::Mesh, Unused, getNormalZkit, false, true>(
                mesh, {}, vertIndex, facePosXm);
        }
        else {
            std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<zenkit::Mesh, Unused, getNormalZkit, false, false>(
                mesh, {}, vertIndex, facePosXm);
        }
        normalStats += faceNormalStats;

        // other
        array<VertexPos, 3> facePos;
        array<VertexBasic, 3> faceOther;

        for (uint32_t i = 0; i < 3; i++) {
            VertexPos pos = toVec3(facePosXm[i]);

            const auto& featureZkit = mesh.features.at(mesh.polygons.feature_indices.at(vertIndex));
            VertexBasic other;
            other.normal = toVec3(faceNormalsXm[i]);
            const auto& glmUvDiffuse = featureZkit.texture;
            other.uvDiffuse = toUv(glmUvDiffuse);
            other.colLight = fromSRGB(Color(featureZkit.light));
            other.dirLight = { -100.f, -100.f, -100.f };// value that is easy to check as not normalized in shader
            other.lightSun = 1.0f;
            other.uvLightmap = getLightmapUvsZkit(mesh, faceIndex, facePosXm[i]);

            facePos.at(2 - i) = pos;
            faceOther.at(2 - i) = other;
            vertIndex++;
        }
        const ChunkIndex chunkIndex = toChunkIndex(centroidPos(facePosXm));
        return { facePos, faceOther, chunkIndex };
    }

    void loadWorldMeshActual(
        MatToChunksToVertsBasic& target,
        const zenkit::Mesh& worldMesh,
        bool indexed,
        bool debugChecksEnabled)
    {
        // It would make sense for this method to return the vector, but for now we just want to 100% sure
        // returning does not copy even in debug mode, and also this is more consistent with other load methods.
        assert(target.empty());

        if (isMeshEmptyAndValidateZkit(worldMesh, true)) {
            ::util::throwError("World mesh is empty!");
        }
        NormalsStats normalStats;
        uint32_t faceCountTotal = worldMesh.polygons.material_indices.size();

        // Sort face indices by material
        unordered_map<uint32_t, vector<uint32_t>> matIndexToFaceIndex;

        for (uint32_t currentFaceIndex = 0; currentFaceIndex < faceCountTotal; currentFaceIndex++) {
            uint32_t matIndex = worldMesh.polygons.material_indices.at(currentFaceIndex);
            auto [it, wasInserted] = matIndexToFaceIndex.try_emplace(matIndex);
            it->second.push_back(currentFaceIndex);
        }

        // Convert to our material and merge identical materials
        // Note: Original data does contain essentially duplicated materials where only material name might differ
        unordered_map<Material, vector<uint32_t>> matToFaceIndex;

        for (auto& [matIndex, faceIndices] : matIndexToFaceIndex) {
            zenkit::Material meshMat = worldMesh.materials.at(matIndex);
            const optional<Material> materialOpt = createMaterial(meshMat, debugChecksEnabled);
            if (materialOpt.has_value()) {
                auto [it, wasInserted] = matToFaceIndex.try_emplace(materialOpt.value());
                auto& mergedFaceIndices = it->second;
                mergedFaceIndices.insert(mergedFaceIndices.end(), faceIndices.begin(), faceIndices.end());
            }
        }

        // Per material: load vertex data and calculate chunkIndex
        for (auto& [material, faceIndices] : matToFaceIndex) {
            uint32_t faceCountMat = faceIndices.size();
            uint32_t vertexCountMat = faceCountMat * 3;

            auto [it, wasInserted] = target.try_emplace(material);
            assert(wasInserted);
            unordered_map<ChunkIndex, VertsBasic>& chunks = it->second;

            // In theory we could write all data to target (chunks) directly for world mesh, because worldmesh
            // is only loaded once, but to keep things consistent with VOB loading, we use temporary buffers here also.
            unordered_map<ChunkIndex, VertsBasic> chunksTemp;

            // Create vertex data and group by chunkIndex
            for (uint32_t i = 0; i < faceCountMat; i++)
            {
                uint32_t currentFace = faceIndices.at(i);
                uint32_t currentVert = currentFace * 3;

                const Face face = loadWorldFace(worldMesh, currentFace, currentVert, debugChecksEnabled, normalStats);
                auto [it, wasInserted] = chunksTemp.try_emplace(face.chunkIndex);
                auto& vertsTemp = it->second;
                if (wasInserted) {
                    // initialize with space relative to full material data to hopefully keep resizing reallocations low
                    uint32_t estimatedPerChunkSize = vertexCountMat * .25f + 1;
                    vertsTemp.vecPos.reserve(estimatedPerChunkSize);
                    vertsTemp.vecOther.reserve(estimatedPerChunkSize);
                }
                insertFace(vertsTemp, face.pos, face.features);// maybe use struct Face as parameter directly?
            }

            // Per material and chunkIndex: generate indices and optimize vertex and index data with meshoptimizer
            for (auto& [chunkIndex, vertsTemp] : chunksTemp) {
                uint32_t vertexCount = vertsTemp.vecPos.size();

                // Create indices to reduce vertexCount, optimize
                if (indexed) {
                    uint32_t indexCount = vertexCount;
                    vertexCount = createIndicesAndRemap(vertsTemp, vertexCount);
                    if (meshoptOptimize) {
                        optimizeIndicesAndVerts(vertsTemp, indexCount, vertexCount);
                    }
                }

                auto [it, wasInserted] = chunks.try_emplace(chunkIndex);
                assert(wasInserted);
                VertsBasic& verts = it->second;
                    
                if (indexed) {
                    verts.useIndices = true;
                    verts.vecIndex = std::move(vertsTemp.vecIndex);
                }
                // we copy vertex data so we don't have to shrink_to_fit (since initial capacity was just a guess)
                verts.vecPos = vertsTemp.vecPos;
                verts.vecOther = vertsTemp.vecOther;
            }
        }

        if (debugChecksEnabled) {
            loadStats.normalsWorldMesh = normalStats;
        }
    }

    void loadWorldMesh(
        MatToChunksToVertsBasic& target,
        const zenkit::Mesh& worldMesh,
        bool indexed,
        bool debugChecksEnabled)
    {
        uint32_t instances = 1;
        for (uint32_t i = 0; i < instances; ++i) {
            loadWorldMeshActual(target, worldMesh, indexed, debugChecksEnabled);
        }
    }

    // ###########################################################################
    // LOAD VOB
    // ###########################################################################

    XMMATRIX inversedTransposed(const XMMATRIX& source) {
        const XMMATRIX transposed = XMMatrixTranspose(source);
        return XMMatrixInverse(nullptr, transposed);
    }

    // TODO
    // - split worldmesh and VOB vertex feature types
    // - split worldmesh and VOB shaders using includes for common functionality
    // - There should be 2 different types:
    //   - FullVert/Vert (Unpacked or Packed)
    //   - Vert/SmallVert/CompVert (Compressed)
    // 
    // function: precompute(zenkit:MRMesh) -> map<Material, UnpackedVerts>
    // - create material-keyed hashmap
    // - per submesh:
    //   - create material
    // - per submesh, per face:
    //   - face positions (non-transformed)
    //     - convert to XMFLOAT4
    //     - scaling
    //   - face normals (non-transformed), requires face positions
    //     - convert to XMFLOAT4
    //     - debugChecks
    //   - set texColor UVs, compress (separate vertex buffers for UVs?)
    //   - get buffer for current material, reserve for additional size and insert
    // - return hashmap
    // 
    // function: indexAndOptimize(vector<UnpackedVerts>) -> (vector<PackedVerts>, vector<Indices>)
    //   - create indices
    //   - optimize indices and vertex buffers
    //   - Optional: Also return a lodded Version of the result
    // 
    // function: compress(UnpackedVert/PackedVert) -> CompressedVert
    //   - compress position (XMFLOAT4 to Vec3)
    //   - compress normal (XMFLOAT4 to Vec3)
    //   - TODO compress other vertex features
    //   - TODO compress by quantizing to necessary bits
    // 
    // CPU-Batching (per instance), mostly inside chunk:
    // - populate cache (first time only):
    //   - run "precompute"
    //   - per material, run "indexAndOptimize"
    //   - cache result
    // - load cached visual
    // - per material, allocate pre-reserved temp CompressedVerts buffer
    // - per material, per vertex:
    //   - transform position
    //   - transform normal
    //   - add per-instance vertex featurs (TODO: move into per-instance structured buffer)
    //   - run "compress"
    //   - insert into temp buffer
    // - get target buffer for chunkIndex of instance
    // - pre-reserve (TODO: test speed) and insert temp buffer into target buffer
    //
    // Optional:
    // CPU-Batching (per instance), crossing chunk boundaries substantially:
    // - run "precompute" to get per-material hashmap with unpacked faces
    // - allocate temp per-material, per-chunk UnpackedVerts buffers
    // - per material, per face:
    //   - transform face positions
    //   - transform face normals
    //   - add per-instance vertex features (TODO: move into per-instance structured buffer)
    //   - calculate centroid to get per-face chunk index
    //   - insert into temp UnpackedVerts buffer
    // - allocate pre-reserved temp CompressedVerts buffer (per chunk)
    // - per material, per chunk:
    //   - run "indexAndOptimize"
    //   - run "compress" per vertex
    //   - insert into temp CompressedVerts buffer
    // - get targetbuffer (per chunk)
    // - pre-reserve (TODO: test speed) and insert temp PackedVerts buffer into target buffer (per-chunk)
    //
    // - create namespace assets::mesh
    // - create files:
    //   - LoadMesh.h
    //   - LoadMeshCommon.h/cpp: functions shared between worldmesh and objects
    //   - LoadMeshWorld.cpp: world mesh loading
    //   - LoadMeshVob.cpp: instance mesh loading

    Face loadVobFace(
        const zenkit::MultiResolutionMesh& mesh,
        const zenkit::SubMesh& submesh,
        const StaticInstance& instance,
        uint32_t faceIndex,
        uint32_t vertIndex,
        bool debugChecksEnabled,
        NormalsStats& normalStats)
    {
        using Mesh = zenkit::MultiResolutionMesh;
        using Submesh = zenkit::SubMesh;

        // positions
        array<XMVECTOR, 3> facePosXm = facePosToXm<Mesh, Submesh, true, true>(
            mesh, submesh, faceIndex, vertIndex, instance.transform);

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
        array<VertexPos, 3> facePos;
        array<VertexBasic, 3> faceOther;

        for (uint32_t i = 0; i < 3; i++) {
            VertexPos pos = toVec3(facePosXm[i]);
            VertexBasic other;
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
        return { facePos, faceOther, chunkIndex };
    }

    struct VertexPrecomp {
        XMFLOAT4 pos;
        XMFLOAT4 normal;
        Uv uvColor;
    };

    array<VertexPrecomp, 3> precomputeFace(
        const zenkit::MultiResolutionMesh& mesh,
        const zenkit::SubMesh& submesh,
        const uint32_t faceIndex,
        const uint32_t vertIndex,
        bool debugChecksEnabled,
        NormalsStats& normalStats
    ) {
        using Mesh = zenkit::MultiResolutionMesh;
        using Submesh = zenkit::SubMesh;

        // positions
        array<XMVECTOR, 3> facePosXm = facePosToXm<Mesh, Submesh, true, false>(mesh, submesh, faceIndex, vertIndex, {});

        // normals
        array<XMVECTOR, 3> faceNormalsXm;
        NormalsStats faceNormalStats;
        if (debugChecksEnabled) {
            std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<Unused, Submesh, getNormalModelZkit, false, true>(
                {}, submesh, vertIndex, facePosXm, {});
        }
        else {
            std::tie(faceNormalStats, faceNormalsXm) = faceNormalsToXm<Unused, Submesh, getNormalModelZkit, false, false>(
                {}, submesh, vertIndex, facePosXm, {});
        }
        normalStats += faceNormalStats;

        // UVs, convert, flip
        array<VertexPrecomp, 3> face;
        for (uint32_t i = 0; i < 3; i++) {
            // flip faces (seems like zEngine uses counter-clockwise winding, while we use clockwise winding)
            // TODO use D3D11_RASTERIZER_DESC FrontCounterClockwise instead?
            VertexPrecomp& vert = face.at(2 - i);

            XMStoreFloat4(&vert.pos, facePosXm[i]);
            XMStoreFloat4(&vert.normal, faceNormalsXm[i]);
            vert.uvColor = getUvModelZkit({}, submesh, vertIndex + i);
        }

        return face;
    }

    struct VertsPrecomp {
        vector<VertexPrecomp> verts;
    };

    optional<unordered_map<Material, VertsPrecomp>> precompute(
        const zenkit::MultiResolutionMesh& mesh, const string& visualName, bool debugChecksEnabled)
    {
        if (mesh.sub_meshes.size() == 0) {
            LOG(WARNING) << "Model contained no geometry! " << visualName;
            return std::nullopt;
        }

        NormalsStats normalStats;
        bool createNormalStats = debugChecksEnabled && !util::hasKey(loadStats.normalsInstances, visualName);

        unordered_map<Material, VertsPrecomp> result;

        for (const auto& submesh : mesh.sub_meshes) {
            zenkit::Material meshMat = submesh.mat;
            if (meshMat.texture.empty() && !util::hasKey(loadStats.skippedNoTexSubmeshInstances, visualName)) {
                loadStats.skippedNoTexSubmeshInstances[visualName] = true;
                continue;
            }
            const optional<Material> materialOpt = createMaterial(meshMat, debugChecksEnabled);
            if (!materialOpt.has_value()) {
                continue;
            }

            uint32_t faceCountMat = submesh.triangles.size();
            uint32_t vertexCountMat = faceCountMat * 3;

            auto [it, wasInserted] = result.try_emplace(materialOpt.value());
            auto& verts = it->second;
            if (wasInserted) {
                verts.verts.reserve(verts.verts.size() + vertexCountMat);
            }

            for (uint32_t currentFace = 0, currentVert = 0;
                currentFace < faceCountMat;
                currentFace++, currentVert += 3) {

                const auto face = precomputeFace(mesh, submesh, currentFace, currentVert, debugChecksEnabled, normalStats);
                verts.verts.push_back(face[0]);
                verts.verts.push_back(face[1]);
                verts.verts.push_back(face[2]);
            }
        }

        return result;
    }

    // TODO maybe debug stats should be part of value
    unordered_map<string, unordered_map<Material, VertsPrecomp>> cachePrecomputedMeshes;
    // TODO precomputeorget

    // function: indexAndOptimize(vector<UnpackedVerts>) -> (vector<PackedVerts>, vector<Indices>)
    //   - create indices
    //   - optimize indices and vertex buffers
    //   - Optional: Also return a lodded Version of the result

    struct IndexingResult {
        VertsPrecomp vertsPacked;
        vector<VertexIndex> indices;
        vector<VertexIndex> indicesLod1;
    };

    template<typename T>
    void meshopt_remapVertexBuffer(vector<T>& target, const vector<T>& source, const vector<uint32_t>& remap)
    {

    }

    void meshoptApplyRemap(VertsPrecomp& verts, vector<VertexIndex> indices, const vector<unsigned int>& remap, uint32_t newIndexCount, uint32_t previousVertCount, uint32_t newVertCount)
    {
        const unsigned int* indicesDataOrNullPtr = indices.empty() ? nullptr : indices.data();
        if (indices.empty()) {
            indices.resize(newIndexCount);
        }
        meshopt_remapIndexBuffer(indices.data(), indicesDataOrNullPtr, newIndexCount, remap.data());
        meshopt_remapVertexBuffer(verts.verts.data(), verts.verts.data(), previousVertCount, sizeof(VertexPrecomp), remap.data());
        if (newVertCount != previousVertCount) {
            assert(newVertCount < previousVertCount);
            verts.verts.resize(newVertCount);
        }
    }

    std::pair<vector<VertexIndex>, uint32_t> createIndicesAndRemap(VertsPrecomp& verts, uint32_t indexCount)
    {
        // assert that we are dealing with entirely unindex vertex data
        //assert(verts.vecIndex.empty());
        assert(verts.verts.size() == indexCount);

        array streams = {
            meshopt_Stream {verts.verts.data(), sizeof(VertexPrecomp), sizeof(VertexPrecomp)},
        };
        std::vector<unsigned int> remap(indexCount); // temporary remap table
        uint32_t vertexCount = meshopt_generateVertexRemapMulti(remap.data(), nullptr, indexCount, indexCount, streams.data(), streams.size());
        meshoptApplyRemap(verts, remap, indexCount, indexCount, vertexCount);

        return vertexCount;
    }

    // takes ownership of vertsUnpacked to move it
    IndexingResult indexAndOptimize(VertsPrecomp& vertsUnpacked, uint32_t indicesOffset = 0)
    {
        IndexingResult result;
        result.vertsPacked = std::move(vertsUnpacked);

        uint32_t vertexCount = vertsUnpacked.verts.size();
        uint32_t indexCount = vertexCount;
        std::tie(result.indices, vertexCount) = createIndicesAndRemap(vertsUnpacked, indexCount);
        if (meshoptOptimize) {
            optimizeIndicesAndVerts(vertsUnpacked, indexCount, vertexCount);
        }
    }

    void loadInstanceMesh(
        MatToChunksToVertsBasic& target,
        const zenkit::MultiResolutionMesh& mesh,
        const StaticInstance& instance,
        bool indexed,
        bool debugChecksEnabled)
    {
        // TODO
        // There should be an instance mesh cache, and for meshes whose bbox are (close to being) inside a chunk, we can just reuse that.
        // For big objects crossing chunk boundary, we could still sort every face into chunks individually. Measure factor by which every
        // chunk bbox is "oversized" on average.
        // Also, callling code should sort calls by visual name so we can clear cache after each visual to keep peek memory usage low.

        using Mesh = zenkit::MultiResolutionMesh;
        using Submesh = zenkit::SubMesh;

        NormalsStats normalStats;
        bool createNormalStats = debugChecksEnabled && !util::hasKey(loadStats.normalsInstances, instance.visual_name);
        
        if (mesh.sub_meshes.size() == 0) {
            LOG(WARNING) << "Model contained no geometry! " << instance.visual_name;
            return;
        }

        for (const auto& submesh : mesh.sub_meshes) {

            // TODO check empty geometry?

            zenkit::Material meshMat = submesh.mat;
            if (meshMat.texture.empty() && !util::hasKey(loadStats.skippedNoTexSubmeshInstances, instance.visual_name)) {
                loadStats.skippedNoTexSubmeshInstances[instance.visual_name] = true;
                continue;
            }
            const optional<Material> materialOpt = createMaterial(meshMat, debugChecksEnabled);
            if (!materialOpt.has_value()) {
                continue;
            }
            unordered_map<ChunkIndex, VertsBasic>& chunks = util::getOrCreateDefault(target, materialOpt.value());

            uint32_t faceCountMat = submesh.triangles.size();
            uint32_t vertexCountMat = faceCountMat * 3;

            unordered_map<ChunkIndex, VertsBasic> chunksTemp;

            for (uint32_t currentFace = 0, currentVert = 0;
                currentFace < faceCountMat;
                currentFace++, currentVert += 3) {
                
                const Face face = loadVobFace(mesh, submesh, instance, currentFace, currentVert, debugChecksEnabled, normalStats);
                auto [it, wasInserted] = chunksTemp.try_emplace(face.chunkIndex);
                auto& vertsTemp = it->second;
                if (wasInserted) {
                    // initialize with space relative to full material data to hopefully keep resizing reallocations low
                    uint32_t estimatedPerChunkSize = vertexCountMat;
                    vertsTemp.vecPos.reserve(estimatedPerChunkSize);
                    vertsTemp.vecOther.reserve(estimatedPerChunkSize);
                }
                insertFace(vertsTemp, face.pos, face.features);// maybe use struct Face as parameter directly?
            }

            // Per material and chunkIndex: generate indices and optimize vertex and index data with meshoptimizer
            for (auto& [chunkIndex, vertsTemp] : chunksTemp) {

                auto [it, wasInserted] = chunks.try_emplace(chunkIndex);
                VertsBasic& vertsTarget = it->second;
                if (wasInserted) {
                    vertsTarget.useIndices = indexed;
                }
                assert(vertsTarget.useIndices == indexed);

                // Create indices to reduce vertexCount, optimize
                if (indexed) {
                    uint32_t vertexCount = vertsTemp.vecPos.size();
                    uint32_t indexCount = vertexCount;
                    vertexCount = createIndicesAndRemap(vertsTemp, vertexCount);
                    if (meshoptOptimize) {
                        optimizeIndicesAndVerts(vertsTemp, indexCount, vertexCount);
                    }

                    // Rewrite indices offset due to already added vert data
                    if (!vertsTarget.vecPos.empty()) {
                        uint32_t currentTargetVertexCount = vertsTarget.vecPos.size();
                        for (uint32_t currentIndex = 0; currentIndex < indexCount; currentIndex++) {
                            VertexIndex& index = vertsTemp.vecIndex.at(currentIndex);
                            index = index + currentTargetVertexCount;
                        }
                    }
                }

                if (indexed) {
                    util::insert(vertsTarget.vecIndex, vertsTemp.vecIndex);
                }
                util::insert(vertsTarget.vecPos, vertsTemp.vecPos);
                util::insert(vertsTarget.vecOther, vertsTemp.vecOther);
            }
        }

        if (debugChecksEnabled) {
            if (createNormalStats) {
                loadStats.normalsInstances[instance.visual_name] = normalStats;
            }
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

    void loadInstanceModel(
        MatToChunksToVertsBasic& target,
        const zenkit::ModelHierarchy& hierarchy,
        const zenkit::ModelMesh& model,
        const StaticInstance& instance,
        bool indexed,
        bool debugChecksEnabled)
    {
        // It's unclear why negation is needed and if only z component or other components needs to be negated as well.
        // If z is not negated, shisha's in Gomez' throneroom (G1) are not placed correctly.
        XMMATRIX transformRoot = XMMatrixTranslationFromVector(toXM4Dir(hierarchy.root_translation) * -1 * G_ASSET_RESCALE);
        for (const auto& mesh : model.meshes) {
            // TODO softskin animation
            StaticInstance newInstance = instance;
            newInstance.transform = XMMatrixMultiply(transformRoot, instance.transform);
            loadInstanceMesh(target, mesh.mesh, newInstance, indexed, debugChecksEnabled);
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
            loadInstanceMesh(target, mesh, newInstance, indexed, debugChecksEnabled);
        }
    }

    /**
     * Create quad.
     */
    vector<std::array<std::pair<XMVECTOR, Uv>, 3>> createQuadPositions(const Decal& decal)
    {
        auto& size = decal.quad_size;
        auto& offset = decal.uv_offset;
        float base = 1.f;
        std::pair quadA = { toXM4Pos(Vec3{ -base * size.x,  base * size.y, 0 }), Uv{ 0 + offset.u, 0 + offset.v } };
        std::pair quadB = { toXM4Pos(Vec3{  base * size.x,  base * size.y, 0 }), Uv{ 1 + offset.u, 0 + offset.v } };
        std::pair quadC = { toXM4Pos(Vec3{  base * size.x, -base * size.y, 0 }), Uv{ 1 + offset.u, 1 + offset.v } };
        std::pair quadD = { toXM4Pos(Vec3{ -base * size.x, -base * size.y, 0 }), Uv{ 0 + offset.u, 1 + offset.v } };

        vector<std::array<std::pair<XMVECTOR, Uv>, 3>> result;
        result.reserve(decal.two_sided ? 4 : 2);

        // vertex order reversed per tri to be consistent with other Gothic asset (counter-clockwise winding)
        result.push_back({ quadA, quadB, quadC });
        result.push_back({ quadA, quadC, quadD });
        if (decal.two_sided) {
            result.push_back({ quadA, quadD, quadC });
            result.push_back({ quadA, quadC, quadB });
        }
        return result;
    }

    void loadInstanceDecal(
        MatToChunksToVertsBasic& target,
        const StaticInstance& instance,
        bool debugChecksEnabled
    )
    {
        assert(instance.decal.has_value());
        auto decal = instance.decal.value();

        const optional<Material> materialOpt = createMaterialDecal(instance.visual_name, decal, debugChecksEnabled);
        if (!materialOpt.has_value()) {
            return;
        }
;       unordered_map<ChunkIndex, VertsBasic>& materialData = ::util::getOrCreateDefault(target, materialOpt.value());

        auto quadFacesXm = createQuadPositions(decal);

        for (auto& quadFaceXm : quadFacesXm) {
            // positions
            array<XMVECTOR, 3> facePosXm;
            for (uint32_t i = 0; i < 3; i++) {
                facePosXm[i] = quadFaceXm.at(i).first;
                facePosXm[i] = XMVectorMultiply(facePosXm[i], XMVectorSet(G_ASSET_RESCALE, G_ASSET_RESCALE, G_ASSET_RESCALE, 1.f));
                facePosXm[i] = XMVector4Transform(facePosXm[i], instance.transform);
            }

            //normals
            const auto faceNormal = toVec3(calcFlatFaceNormal(facePosXm));

            //other
            array<VertexPos, 3> facePos;
            array<VertexBasic, 3> faceOther;

            for (int32_t i = 2; i >= 0; i--) {
                facePos[i] = toVec3(facePosXm.at(i));

                VertexBasic other;
                other.normal = faceNormal;
                other.uvDiffuse = quadFaceXm.at(i).second;
                other.uvLightmap = { 0, 0, -1 };
                other.colLight = instance.lighting.color;
                other.dirLight = faceNormal;// TODO does the original game do this??
                other.lightSun = instance.lighting.receiveLightSun ? 1.f : 0.f;
                faceOther[i] = other;
            }

            const ChunkIndex chunkIndex = toChunkIndex(centroidPos(facePosXm));
            VertsBasic& chunkData = ::util::getOrCreateDefault(materialData, chunkIndex);
            insertFace(chunkData, facePos, faceOther);
        }
    }

    void printAndResetLoadStats(bool debugChecksEnabled)
    {
        LOG(INFO) << "Skipped mesh data:";
        for (auto& [name, __] : loadStats.skippedNoTexSubmeshInstances) {
            LOG(WARNING) << "    Skipped VOB submesh because of empty texture! " << name;
        }
        if (debugChecksEnabled) {
            LOG(INFO) << "Norrmal Stats:";
            logNormalStats(loadStats.normalsWorldMesh, "LEVEL_MESH");
            for (auto& [name, normals] : loadStats.normalsInstances) {
                logNormalStats(normals, name);
            }
            LOG(INFO) << "Material count by group:";
            for (auto& [group, count] : loadStats.materialGroups) {
                LOG(INFO) << "     '" << magic_enum::enum_name(group) << "' -> " << std::to_string(count);
            }
            LOG(INFO) << "Material count by alpha (incl. decals):";
            for (auto& [alpha, count] : loadStats.materialAlphas) {
                LOG(INFO) << "     '" << magic_enum::enum_name(alpha) << "' -> " << std::to_string(count);
            }
        }

        loadStats.normalsInstances.clear();
        loadStats.materialGroups.clear();
        loadStats.materialAlphas.clear();
    }
}