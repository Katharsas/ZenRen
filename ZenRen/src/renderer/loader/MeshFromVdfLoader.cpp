#include "stdafx.h"
#include "MeshFromVdfLoader.h"

#include <filesystem>

#include "../../Util.h"

namespace renderer::loader {

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

    UV from(const ZenLib::ZMath::float2& source)
    {
        return UV { source.x, source.y };
    }

    VEC3 from(const ZenLib::ZMath::float3& source)
    {
        return VEC3 { source.x, source.y, source.z };
    }

    template <typename T> bool isZero(const T& vec3, float threshold)
    {
        return std::abs(vec3.x) <= threshold && std::abs(vec3.y) <= threshold && std::abs(vec3.z) <= threshold;
    }
    template <typename T> XMVECTOR toXM4Pos(const T& vec3)
    {
        return XMVectorSet(vec3.x, vec3.y, vec3.z, 1);
    }
    template <typename T> XMVECTOR toXM4Dir(const T& vec3)
    {
        return XMVectorSet(vec3.x, vec3.y, vec3.z, 0);
    }

    VEC3 toVec3(const XMVECTOR& xm4)
    {
        XMFLOAT4 result;
        XMStoreFloat4(&result, xm4);
        return VEC3{ result.x, result.y, result.z };
    }

    VEC4 toVec4(const XMVECTOR& xm4)
    {
        XMFLOAT4 result;
        XMStoreFloat4(&result, xm4);
        return VEC4{ result.x, result.y, result.z, result.w };
    }

    inline std::ostream& operator <<(std::ostream& os, const XMVECTOR& that)
    {
        XMFLOAT4 float4;
        XMStoreFloat4(&float4, that);
        return os << "[X:" << float4.x << " Y:" << float4.y << " Z:" << float4.z << " W:" << float4.w << "]";
    }

    void warnIfNotNormalized(const XMVECTOR& source)
    {
        XMVECTOR normalized = XMVector3Normalize(source);
        XMVECTOR nearEqualMask = XMVectorNearEqual(source, normalized, XMVectorReplicate(.0001f));
        XMVECTOR nearEqualXm = XMVectorSelect(XMVectorReplicate(1.f), XMVectorReplicate(.0f), nearEqualMask);
        VEC4 nearEqual = toVec4(nearEqualXm);
        if (nearEqual.x != 0 || nearEqual.y != 0 || nearEqual.z != 0 || nearEqual.w != 0) {
            LOG(INFO) << "Vector was not normalized! " << source << "  |  " << normalized << "  |  " << nearEqualXm;
        }
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
    const std::string normalizeTextureName(const std::string& texture) {
        auto texFilepath = std::filesystem::path(texture);
        auto texFilename = util::toString(texFilepath.filename());
        ::util::asciiToLower(texFilename);
        return texFilename;
    }

    void loadWorldMesh(
        unordered_map<Material, vector<WORLD_VERTEX>>& target,
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

            vector<WORLD_VERTEX> faces;

            int32_t faceIndex = 0;
            for (int32_t indicesIndex = 0; indicesIndex < submesh.indices.size(); indicesIndex += 3) {

                const array zenFace = getFaceVerts(packedMesh.vertices, submesh.indices, indicesIndex);

                ZenLoad::Lightmap lightmap;
                const int16_t faceLightmapIndex = submesh.triangleLightmapIndices[faceIndex];
                if (faceLightmapIndex != -1) {
                    lightmap = worldMesh->getLightmapReferences()[faceLightmapIndex];
                }

                array<WORLD_VERTEX, 3> face;

                uint32_t vertexIndex = 0;
                for (const auto& zenVert : zenFace) {
                    WORLD_VERTEX vertex;
                    vertex.pos = from(zenVert.Position);
                    vertex.normal = from(zenVert.Normal);
                    vertex.uvDiffuse = from(zenVert.TexCoord);

                    if (faceLightmapIndex == -1) {
                        vertex.uvLightmap = { 0, 0, -1 };
                    }
                    else {
                        float unscale = 100;
                        XMVECTOR pos = XMVectorSet(vertex.pos.x * unscale, vertex.pos.y * unscale, vertex.pos.z * unscale, 0);
                        XMVECTOR origin = XMVectorSet(lightmap.origin.x, lightmap.origin.y, lightmap.origin.z, 0);
                        XMVECTOR normalUp = XMVectorSet(lightmap.normalUp.x, lightmap.normalUp.y, lightmap.normalUp.z, 0);
                        XMVECTOR normalRight = XMVectorSet(lightmap.normalRight.x, lightmap.normalRight.y, lightmap.normalRight.z, 0);
                        XMVECTOR lightmapDir = pos - origin;
                        vertex.uvLightmap.u = XMVectorGetX(XMVector3Dot(lightmapDir, normalRight));
                        vertex.uvLightmap.v = XMVectorGetX(XMVector3Dot(lightmapDir, normalUp));
                        vertex.uvLightmap.i = lightmap.lightmapTextureIndex;
                    }
                    face.at(2 - vertexIndex) = vertex;// flip faces apparently, but not z ?!
                    vertexIndex++;
                }
                faces.insert(faces.end(), face.begin(), face.end());
                faceIndex++;
            }

            const auto& texture = submesh.material.texture;
            if (!texture.empty()) {
                Material material = { normalizeTextureName(texture) };
                auto& matVertices = getOrCreate(target, material);
                matVertices.insert(matVertices.end(), faces.begin(), faces.end());
            }
        }
    }

    unordered_set<string> processedVisuals;

	void loadInstanceMesh(
        unordered_map<Material, vector<POS_NORMAL_UV>>& target,
        const ZenLoad::zCProgMeshProto& mesh,
        const XMMATRIX& transform,
        const string& visualname,
        bool debugChecksEnabled)
    {
        ZenLoad::PackedMesh packedMesh;
        mesh.packMesh(packedMesh, 0.01f);

        XMMATRIX normalTransform = inversedTransposed(transform);

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

            vector<POS_NORMAL_UV> faces;
            for (uint32_t indicesIndex = 0; indicesIndex < submesh.indices.size(); indicesIndex += 3) {
                
                const array zenFace = getFaceVerts(packedMesh.vertices, submesh.indices, indicesIndex);
                array<POS_NORMAL_UV, 3> face;

                // read positions
                array<XMVECTOR, 3> posXm;
                for (int32_t i = 0; i < face.size(); i++) {
                    const auto& zenVert = zenFace[i];
                    posXm[i] = toXM4Pos(zenVert.Position);
                }

                // read normals, set to flat if zero
                array<XMVECTOR, 3> normalsXm;

                // counter-clockwise winding, flip XMVector3Cross argument order for clockwise winding
                XMVECTOR faceNormalXm = XMVector3Cross(XMVectorSubtract(posXm[2], posXm[0]), XMVectorSubtract(posXm[1], posXm[0]));
                for (int32_t i = 0; i < face.size(); i++) {
                    const auto& zenVert = zenFace[i];
                    if (debugChecksEnabled && isZero(zenVert.Normal, zeroThreshold)) {
                        normalsXm[i] = faceNormalXm;
                        zeroNormals++;
                    }
                    else {
                        normalsXm[i] = toXM4Dir(zenVert.Normal);
                    }
                    totalNormals++;
                }

                // detect wrong normals
                if (debugChecksEnabled) {
                    for (int32_t i = 0; i < face.size(); i++) {
                        XMVECTOR angleXm = XMVector3AngleBetweenNormals(faceNormalXm, normalsXm[i]);
                        float normalFlatnessRadian = XMVectorGetX(angleXm);
                        float normalFlatnessDegrees = normalFlatnessRadian * (180.0 / 3.141592653589793238463);
                        if (normalFlatnessDegrees > 90) {
                            extremeNormals++;
                        }
                    }
                }

                // transform
                for (int32_t i = 0; i < face.size(); i++) {
                    const auto& zenVert = zenFace[i];
                    POS_NORMAL_UV vertex;
                    vertex.pos = transformPos(posXm[i], transform);
                    vertex.normal = transformDir(normalsXm[i], normalTransform, debugChecksEnabled);
                    vertex.uvDiffuse = from(zenVert.TexCoord);
                    vertex.uvLightmap = { 0, 0 };

                    // flip faces (seems like zEngine uses counter-clockwise winding, while we use clockwise winding)
                    face.at(2 - i) = vertex;
                }
                faces.insert(faces.end(), face.begin(), face.end());
            }

            const auto& texture = submesh.material.texture;
            if (!texture.empty()) {
                Material material = { normalizeTextureName(texture) };
                auto& matVertices = getOrCreate(target, material);
                matVertices.insert(matVertices.end(), faces.begin(), faces.end());
            }
        }

        if (debugChecksEnabled) {
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