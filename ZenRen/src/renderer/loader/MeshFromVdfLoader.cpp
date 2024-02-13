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

    int32_t wrapModulo(int32_t positiveOrNegative, int32_t modulo)
    {
        return ((positiveOrNegative % modulo) + modulo) % modulo;
    }

    XMMATRIX inversedTransposed(const XMMATRIX& source) {
        const XMMATRIX transposed = XMMatrixTranspose(source);
        return XMMatrixInverse(nullptr, source);
    }

    UV from(const ZenLib::ZMath::float2& source)
    {
        return UV { source.x, source.y };
    }

    VEC3 from(const ZenLib::ZMath::float3& source)
    {
        return VEC3 { source.x, source.y, source.z };
    }

    VEC3 from(const ZenLib::ZMath::float3& source, const XMMATRIX& transform, const bool normalize)
    {
        XMVECTOR pos = XMVectorSet(source.x, source.y, source.z, 1.0f);
        pos = XMVector4Transform(pos, transform);
        if (normalize) {
            pos = XMVector4Normalize(pos);
        }
        return VEC3{ XMVectorGetX(pos), XMVectorGetY(pos), XMVectorGetZ(pos) };
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

	void loadInstanceMesh(
        unordered_map<Material, vector<POS_NORMAL_UV>>& target,
        const ZenLoad::zCProgMeshProto& mesh,
        const XMMATRIX& transform)
    {
        ZenLoad::PackedMesh packedMesh;
        mesh.packMesh(packedMesh, 0.01f);

        XMMATRIX normalTransform = inversedTransposed(transform);

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

                uint32_t vertexIndex = 0;
                for (const auto& zenVert : zenFace) {
                    POS_NORMAL_UV vertex;
                    vertex.pos = from(zenVert.Position, transform, false);
                    vertex.normal = from(zenVert.Normal, normalTransform, true);
                    vertex.uvDiffuse = from(zenVert.TexCoord);
                    vertex.uvLightmap = { 0, 0 };

                    face.at(2 - vertexIndex) = vertex;// flip faces apparently, but not z ?!
                    vertexIndex++;
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
	}
}