#include "stdafx.h"
#include "MeshFromVdfLoader.h"

#include <type_traits>
#include <filesystem>
#include <variant>

#include "AssetCache.h"
#include "render/MeshUtil.h"
#include "../Util.h"

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

    static_assert(Vec3<glm::vec3>);
    static_assert(Vec2<glm::vec2>);

    template<typename MESH, typename SUBMESH>
    concept IS_WORLD_MESH = std::is_same_v<MESH, zenkit::Mesh>;

    template<typename MESH, typename SUBMESH>
    concept IS_MODEL_MESH = std::is_same_v<MESH, zenkit::MultiResolutionMesh> && std::is_same_v<SUBMESH, zenkit::SubMesh>;

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

    template <typename Mesh, typename Submesh>
    using GetNormal = XMVECTOR (*) (const Mesh& mesh, const Submesh& submesh, uint32_t vertIndex);

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

    // ###########################################################################
    // UTIL FUNCTIONS
    // ###########################################################################

    template <typename VERTEX_FEATURE>
    void insertFace(Verts<VERTEX_FEATURE>& target, const array<VertexPos, 3>& facePos, const array<VERTEX_FEATURE, 3>& faceOther) {
        // manual reservation strategy helps with big vectors
        // TODO might also reduce speed massively
        // TODO find out what our actual memory problem is, we should be able to allocate 3.2G!! does DX11 driver take a lot?
        static uint32_t vertReserveCount = 160 * 3;
        if (!target.vecPos.empty() && target.vecPos.size() % vertReserveCount == 0) {
            uint32_t blocks = (target.vecPos.size() / vertReserveCount) + 1;
            target.vecPos.reserve(blocks * vertReserveCount);
            target.vecOther.reserve(blocks * vertReserveCount);
        }
        target.vecPos.push_back(facePos[0]);
        target.vecPos.push_back(facePos[1]);
        target.vecPos.push_back(facePos[2]);
        target.vecOther.push_back(faceOther[0]);
        target.vecOther.push_back(faceOther[1]);
        target.vecOther.push_back(faceOther[2]);
    }

    array<VEC3, 2> createVertlistBbox(const vector<VertexPos>& verts)
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
    // LOAD WORLD
    // ###########################################################################

    void loadWorldFace(
        unordered_map<ChunkIndex, VertsBasic>& target,
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
            other.colLight = fromSRGB(COLOR(featureZkit.light));
            other.dirLight = { -100.f, -100.f, -100.f };// value that is easy to check as not normalized in shader
            other.lightSun = 1.0f;
            other.uvLightmap = getLightmapUvsZkit(mesh, faceIndex, facePosXm[i]);

            facePos.at(2 - i) = pos;
            faceOther.at(2 - i) = other;
            vertIndex++;
        }
        const ChunkIndex chunkIndex = toChunkIndex(centroidPos(facePosXm));
        VertsBasic& chunkData = util::getOrCreateDefault(target, chunkIndex);
        insertFace<VertexBasic>(chunkData, facePos, faceOther);
    }

    void loadWorldMeshActual(
        MatToChunksToVertsBasic& target,
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
            const optional<Material> materialOpt = createMaterial(meshMat, debugChecksEnabled);
            
            uint32_t nextMeshMatIndex;
            if (!materialOpt.has_value()) {
                // skip faces with unsupported material
                do {
                    faceIndex++; vertIndex += 3;
                    if (faceIndex >= faceCount) {
                        break;
                    }
                    nextMeshMatIndex = worldMesh.polygons.material_indices.at(faceIndex);
                } while (nextMeshMatIndex == meshMatIndex);
            }
            else {
                ChunkToVerts<VertexBasic>& vertexData = util::getOrCreateDefault(target, materialOpt.value());
                do {
                    loadWorldFace(vertexData, worldMesh, faceIndex, vertIndex, debugChecksEnabled, normalStats);

                    faceIndex++; vertIndex += 3;
                    if (faceIndex >= faceCount) {
                        break;
                    }
                    nextMeshMatIndex = worldMesh.polygons.material_indices.at(faceIndex);
                } while (nextMeshMatIndex == meshMatIndex);
            }
        }

        if (debugChecksEnabled) {
            loadStats.normalsWorldMesh = normalStats;
        }
    }

    void loadWorldMesh(
        MatToChunksToVertsBasic& target,
        const zenkit::Mesh& worldMesh,
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

    void loadInstanceMesh(
        MatToChunksToVertsBasic& target,
        const zenkit::MultiResolutionMesh& mesh,
        const StaticInstance& instance,
        bool debugChecksEnabled)
    {
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
            unordered_map<ChunkIndex, VertsBasic>& materialData = util::getOrCreateDefault(target, materialOpt.value());

            uint32_t faceCount = submesh.triangles.size();

            for (uint32_t faceIndex = 0, vertIndex = 0; faceIndex < faceCount; faceIndex++) {
                // positions
                array<XMVECTOR, 3> facePosXm = facePosToXm<Mesh, Submesh, true, true>(
                    mesh, submesh, faceIndex, vertIndex, instance.transform);

                // normals
                array<XMVECTOR, 3> faceNormalsXm;
                NormalsStats faceNormalStats;
                if (createNormalStats) {
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
                VertsBasic& chunkData = ::util::getOrCreateDefault(materialData, chunkIndex);
                insertFace(chunkData, facePos, faceOther);
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
        bool debugChecksEnabled)
    {
        // It's unclear why negation is needed and if only z component or other components needs to be negated as well.
        // If z is not negated, shisha's in Gomez' throneroom (G1) are not placed correctly.
        XMMATRIX transformRoot = XMMatrixTranslationFromVector(toXM4Dir(hierarchy.root_translation) * -1 * G_ASSET_RESCALE);
        for (const auto& mesh : model.meshes) {
            // TODO softskin animation
            StaticInstance newInstance = instance;
            newInstance.transform = XMMatrixMultiply(transformRoot, instance.transform);
            loadInstanceMesh(target, mesh.mesh, newInstance, debugChecksEnabled);
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
            loadInstanceMesh(target, mesh, newInstance, debugChecksEnabled);
        }
    }

    /**
     * Create quad.
     */
    vector<std::array<std::pair<XMVECTOR, UV>, 3>> createQuadPositions(const Decal& decal)
    {
        auto& size = decal.quad_size;
        auto& offset = decal.uv_offset;
        float base = 1.f;
        std::pair quadA = { toXM4Pos(VEC3{ -base * size.x,  base * size.y, 0 }), UV{ 0 + offset.u, 0 + offset.v } };
        std::pair quadB = { toXM4Pos(VEC3{  base * size.x,  base * size.y, 0 }), UV{ 1 + offset.u, 0 + offset.v } };
        std::pair quadC = { toXM4Pos(VEC3{  base * size.x, -base * size.y, 0 }), UV{ 1 + offset.u, 1 + offset.v } };
        std::pair quadD = { toXM4Pos(VEC3{ -base * size.x, -base * size.y, 0 }), UV{ 0 + offset.u, 1 + offset.v } };

        vector<std::array<std::pair<XMVECTOR, UV>, 3>> result;
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
                other.dirLight = toVec3(XMVector3Normalize(instance.lighting.direction));
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