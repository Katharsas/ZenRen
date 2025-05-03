#include "stdafx.h"
#include "MeshFromVdfLoader.h"

#include <type_traits>
#include <filesystem>
#include <variant>

#include <cstring>

#include "AssetCache.h"
#include "MeshOpt.h"
#include "render/MeshUtil.h"
#include "../Util.h"

#include "../lib/meshoptimizer/src/meshoptimizer.h"// TODO remove

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
    using ::std::pair;
	using ::util::getOrCreate;

    constexpr bool meshoptOptimize = true;
    constexpr bool manualReservation = false;

    const float zeroThreshold = 0.000001f;
    const XMMATRIX identity = XMMatrixIdentity();
    const XMVECTOR vecZero = XMVectorZero();

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
    // WORLD PACKING
    // ###########################################################################

    template <VERTEX_FEATURE F>
    uint32_t createIndicesAndRemap(Verts<F>& verts)
    {
        uint32_t vertexCount = verts.vecPos.size();
        assert(verts.vecIndex.empty());
        assert(verts.vecOther.size() == vertexCount);

        array streams = {
            meshopt::createStream(verts.vecPos),
            meshopt::createStream(verts.vecOther)
        };
        auto remap = meshopt::generateIndicesRemap(vertexCount, streams);
        verts.vecIndex = meshopt::createIndexBuffer(remap);
        meshopt::remapVertexBuffer(verts.vecPos, remap);
        meshopt::remapVertexBuffer(verts.vecOther, remap);

        return remap.vertCountRemapped;
    }

    template <VERTEX_FEATURE F>
    void optimizeIndicesAndVerts(Verts<F>& verts)
    {
        uint32_t vertexCount = verts.vecPos.size();
        assert(verts.vecIndex.size() >= vertexCount);
        assert(verts.vecOther.size() == vertexCount);

        meshopt::optimizeVertexCache(verts.vecIndex, vertexCount);
        meshopt::optimizeOverdraw(verts.vecIndex, verts.vecPos);

        auto remap = meshopt::optimizeVertexFetchRemap(verts.vecIndex, vertexCount);
        meshopt::remapIndexBuffer(verts.vecIndex, remap);
        meshopt::remapVertexBuffer(verts.vecPos, remap);
        meshopt::remapVertexBuffer(verts.vecOther, remap);
    }

    // ###########################################################################
    // WORLD LOADING
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
                    vertexCount = createIndicesAndRemap(vertsTemp);
                    if (meshoptOptimize) {
                        optimizeIndicesAndVerts(vertsTemp);
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
    // VOB PRECOMPUTE
    // ###########################################################################

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

    using VertsPrecomp = vector<VertexPrecomp>;

    optional<unordered_map<Material, VertsPrecomp>> precompute(
        const zenkit::MultiResolutionMesh& mesh,
        const string& visualName,
        bool debugChecksEnabled)
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
                verts.reserve(verts.size() + vertexCountMat);
            }

            for (uint32_t currentFace = 0, currentVert = 0;
                currentFace < faceCountMat;
                currentFace++, currentVert += 3) {

                const auto face = precomputeFace(mesh, submesh, currentFace, currentVert, debugChecksEnabled, normalStats);
                verts.push_back(face[0]);
                verts.push_back(face[1]);
                verts.push_back(face[2]);
            }
        }

        return result;
    }

    // ###########################################################################
    // VOB PACKING
    // ###########################################################################

    struct VertsPacked {
        VertsPrecomp vertsPacked;
        vector<VertexIndex> indices;
        vector<VertexIndex> indicesLod1;
    };

    vector<VertexIndex> createIndicesAndRemap(VertsPrecomp& verts)
    {
        array streams = { meshopt::createStream(verts) };
        auto remap = meshopt::generateIndicesRemap(verts.size(), streams);
        vector<VertexIndex> indices = meshopt::createIndexBuffer(remap);
        meshopt::remapVertexBuffer(verts, remap);
        return indices;
    }

    void optimizeIndicesAndVerts(VertsPrecomp& verts, vector<VertexIndex>& indices)
    {
        uint32_t vertexCount = verts.size();
        meshopt::optimizeVertexCache(indices, vertexCount);
        meshopt::optimizeOverdraw(indices, verts);
        auto remap = meshopt::optimizeVertexFetchRemap(indices, vertexCount);
        meshopt::remapIndexBuffer(indices, remap);
        meshopt::remapVertexBuffer(verts, remap);
    }

    vector<VertexIndex> createIndicesLod(VertsPrecomp& verts, vector<VertexIndex>& indices)
    {
        uint32_t indexCount = indices.size();

        //float threshold = 0.2f;
        //uint32_t target_index_count = uint32_t(indexCount * threshold);
        uint32_t target_index_count = 0;
        float target_error = 0.01f;

        std::vector<uint32_t> lod(indexCount);
        float lod_error = 0.f;
        uint32_t indexCountLod = meshopt_simplify(&lod.data()[0], indices.data(), indexCount, &verts[0].pos.x, verts.size(), sizeof(VertexPrecomp),
            target_index_count, target_error, /* options= */ 0, &lod_error);
        lod.resize(indexCountLod);
        return lod;
    }

    // takes ownership of vertsUnpacked to move it
    VertsPacked indexAndOptimize(VertsPrecomp& vertsUnpacked, bool generateLod)
    {
        // TODO create LOD
        VertsPacked result;
        result.vertsPacked = std::move(vertsUnpacked);
        result.indices = createIndicesAndRemap(result.vertsPacked);
        if (meshoptOptimize) {
            optimizeIndicesAndVerts(result.vertsPacked, result.indices);
        }
        if (generateLod) {
            result.indicesLod1 = createIndicesLod(result.vertsPacked, result.indices);
            //result.indices = result.indicesLod1;
        }
        return result;
    }

    // TODO maybe debug stats should be part of value
    unordered_map<uint32_t, unordered_map<Material, VertsPacked>> cacheMeshes;

    unordered_map<Material, VertsPacked>& getOrPrecompute(
        uint32_t meshId, bool indexed, bool generateLod, std::function<optional<unordered_map<Material, VertsPrecomp>>()> precompute)
    {
        // get cached vertex attributes and index buffers, or init cache
        auto [it, wasInserted] = cacheMeshes.try_emplace(meshId);
        unordered_map<Material, VertsPacked>& cachedVerts = it->second;
        if (wasInserted) {
            optional<unordered_map<Material, VertsPrecomp>> preVertsOpt = precompute();
            if (preVertsOpt.has_value()) {
                for (auto& [material, verts] : preVertsOpt.value()) {
                    if (indexed) {
                        cachedVerts.emplace(material, std::move(indexAndOptimize(verts, generateLod)));
                    }
                    else {
                        cachedVerts.emplace(material, VertsPacked{ .vertsPacked = std::move(verts) });
                    }
                }
            }
        }
        return cachedVerts;
    }

    // ###########################################################################
    // VOB LOADING
    // ###########################################################################

    template <bool transform>
    pair<VertexPos, VertexBasic> finalizeCompressVertex(
        const VertexPrecomp& vert,
        const StaticInstance& instance,
        bool isDecal,
        OPT_PARAM<transform, const XMMATRIX&> posTransform,
        OPT_PARAM<transform, const XMMATRIX&> normalTransform)
    {
        // TODO compress further by quantizing to necessary bits
        pair<VertexPos, VertexBasic> result;

        XMFLOAT4 pos = vert.pos;
        if constexpr (transform) {
            XMVECTOR posXm = XMVector4Transform(XMLoadFloat4(&pos), posTransform);
            XMStoreFloat4(&pos, posXm);
        }
        XMFLOAT4 normal = vert.normal;
        if constexpr (transform) {
            XMVECTOR normalXm = XMVector3TransformNormal(XMLoadFloat4(&normal), normalTransform);
            normalXm = XMVector3Normalize(normalXm);
            XMStoreFloat4(&normal, normalXm);
        }

        result.first = toVec3(pos);
        result.second = {
            .normal = toVec3(normal),
            .uvDiffuse = vert.uvColor,
            .uvLightmap = { -1, -1, -1 },
            .colLight = instance.lighting.color,
            .lightSun = instance.lighting.receiveLightSun ? 1.f : 0.f,
        };
        if (!isDecal) {
            result.second.dirLight = toVec3(XMVector3Normalize(instance.lighting.direction));
        } else {
            result.second.dirLight = result.second.normal;// TODO does the original game do this??
        }
        return result;
    }

    void instantiateAndInsert(
        MatToChunksToVertsBasic& target,
        const ChunkIndex& chunkIndex,
        const unordered_map<Material, VertsPacked>& verts,
        const StaticInstance& instance,
        bool isDecal)
    {
        // transform pos/normal, compress and copy into target buffer
        for (auto& [material, packed] : verts) {
            bool indexed = !packed.indices.empty();
            auto& chunks = util::getOrCreateDefault(target, material);
            auto [it, wasInserted] = chunks.try_emplace(chunkIndex);
            VertsBasic& targetVerts = it->second;
            if (wasInserted) {
                targetVerts.useIndices = indexed;
                if (indexed) {
                    targetVerts.vecIndex.reserve(300);
                }
                targetVerts.vecPos.reserve(300);
                targetVerts.vecOther.reserve(300);
            }
            assert(targetVerts.useIndices == indexed);

            if (indexed) {
                uint32_t vertOffset = targetVerts.vecPos.size();
                for (uint32_t index : packed.indices) {
                    targetVerts.vecIndex.push_back(vertOffset + index);
                }
            }

            uint32_t vertCount = packed.vertsPacked.size();
            for (const VertexPrecomp& vert : packed.vertsPacked) {
                auto [pos, other] = finalizeCompressVertex<true>(vert, instance, isDecal, instance.transform, instance.transform);
                targetVerts.vecPos.push_back(pos);
                targetVerts.vecOther.push_back(other);
            }
        }
    }

    void loadInstanceMesh(
        MatToChunksToVertsBasic& target,
        const zenkit::MultiResolutionMesh& mesh,
        const StaticInstance& instance,
        uint32_t submeshId,
        bool indexed,
        bool debugChecksEnabled)
    {
        // TODO we cannot just pass all instances of a single visual at once because one visual model
        // might have several meshes. But cache should be cleared after every single-mesh or model.

        NormalsStats normalStats;
        bool createNormalStats = debugChecksEnabled && !util::hasKey(loadStats.normalsInstances, instance.visual_name);

        size_t hash = 0;
        util::hashCombine(hash, instance.visual_name);
        util::hashCombine(hash, submeshId);

        unordered_map<Material, VertsPacked>& cachedVerts = getOrPrecompute((uint32_t)hash, indexed, true, [&]() {
            return precompute(mesh, instance.visual_name, debugChecksEnabled);
        });
        
        if (cachedVerts.empty()) {
            return;
        }

        ChunkIndex chunkIndex = toChunkIndex(bboxCenter(instance.bbox));
        instantiateAndInsert(target, chunkIndex, cachedVerts, instance, false);
    }

    void loadInstanceMesh(
        MatToChunksToVertsBasic& target,
        const zenkit::MultiResolutionMesh& mesh,
        const StaticInstance& instance,
        bool indexed,
        bool debugChecksEnabled)
    {
        // TODO pass all instances of a single mesh together here so we can clear mesh cache after all are done
        loadInstanceMesh(target, mesh, instance, 0, indexed, debugChecksEnabled);
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
        // TODO pass all instances of a single model together here so we can clear mesh cache after all are done
        uint32_t meshId = 0;

        // It's unclear why negation is needed and if only z component or other components needs to be negated as well.
        // If z is not negated, shisha's in Gomez' throneroom (G1) are not placed correctly.
        XMMATRIX transformRoot = XMMatrixTranslationFromVector(toXM4Dir(hierarchy.root_translation) * -1 * G_ASSET_RESCALE);
        for (const auto& mesh : model.meshes) {
            // TODO softskin animation
            StaticInstance newInstance = instance;
            newInstance.transform = XMMatrixMultiply(transformRoot, instance.transform);
            loadInstanceMesh(target, mesh.mesh, newInstance, meshId++, indexed, debugChecksEnabled);
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
            loadInstanceMesh(target, mesh, newInstance, meshId++, indexed, debugChecksEnabled);
        }
    }

    /**
     * Create quad.
     */
    vector<array<pair<XMVECTOR, Uv>, 3>> createQuadPositions(const Decal& decal)
    {
        auto& size = decal.quad_size;
        auto& offset = decal.uv_offset;
        float base = 1.f;
        pair A = { toXM4Pos(Vec3{ -base * size.x,  base * size.y, 0 }), Uv{ 0 + offset.u, 0 + offset.v } };
        pair B = { toXM4Pos(Vec3{  base * size.x,  base * size.y, 0 }), Uv{ 1 + offset.u, 0 + offset.v } };
        pair C = { toXM4Pos(Vec3{  base * size.x, -base * size.y, 0 }), Uv{ 1 + offset.u, 1 + offset.v } };
        pair D = { toXM4Pos(Vec3{ -base * size.x, -base * size.y, 0 }), Uv{ 0 + offset.u, 1 + offset.v } };

        vector<array<pair<XMVECTOR, Uv>, 3>> result;
        result.reserve(decal.two_sided ? 4 : 2);

        // counter-clockwise winding to be consistent with other Gothic assets
        // for indexing to work, vertices that can de decuplicated (A, C) must be adjacent to each other across faces
        result.push_back({ D, C, A });
        result.push_back({ A, C, B });
        if (decal.two_sided) {
            result.push_back({ B, C, A });
            result.push_back({ A, C, D });
        }

        return result;
    }

    VertsPrecomp precomputeDecal(const Decal& decal)
    {
        auto quadFacesXm = createQuadPositions(decal);

        VertsPrecomp result;
        result.reserve(quadFacesXm.size() * 3);

        for (auto& quadFaceXm : quadFacesXm) {
            // positions
            array<XMVECTOR, 3> facePosXm;
            for (uint32_t i = 0; i < 3; i++) {
                facePosXm[i] = quadFaceXm.at(i).first;
                facePosXm[i] = XMVectorMultiply(facePosXm[i], XMVectorSet(G_ASSET_RESCALE, G_ASSET_RESCALE, G_ASSET_RESCALE, 1.f));
            }

            //normal
            XMFLOAT4 faceNormal;
            XMStoreFloat4(&faceNormal, calcFlatFaceNormal(facePosXm));

            // flip faces (seems like zEngine uses counter-clockwise winding, while we use clockwise winding)
            // TODO use D3D11_RASTERIZER_DESC FrontCounterClockwise instead?
            for (int32_t i = 2; i >= 0; i--) {
                 VertexPrecomp vert;
                 XMStoreFloat4(&vert.pos, facePosXm[i]);
                 vert.normal = faceNormal;
                 vert.uvColor = quadFaceXm[i].second;
                 result.push_back(vert);
            }
        }
        return result;
    }

    void loadInstanceDecal(
        MatToChunksToVertsBasic& target,
        const StaticInstance& instance,
        bool indexed,
        bool debugChecksEnabled
    )
    {
        assert(instance.decal.has_value());
        auto decal = instance.decal.value();

        const optional<Material> materialOpt = createMaterialDecal(instance.visual_name, decal, debugChecksEnabled);
        if (!materialOpt.has_value()) {
            return;
        }
        const Material& material = materialOpt.value();
        
        VertsPrecomp vertsPre = precomputeDecal(decal);

        unordered_map<Material, VertsPacked> vertsPacked;
        if (indexed) {
            vertsPacked.emplace(material, std::move(indexAndOptimize(vertsPre, false)));
        }
        else {
            vertsPacked.emplace(material, VertsPacked{ .vertsPacked = std::move(vertsPre) });
        }

        ChunkIndex chunkIndex = toChunkIndex(bboxCenter(instance.bbox));
        instantiateAndInsert(target, chunkIndex, vertsPacked, instance, true);
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