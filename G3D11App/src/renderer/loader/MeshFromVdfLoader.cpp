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
	using ::util::getOrCreate;

    int32_t wrapModulo(int32_t positiveOrNegative, int32_t modulo)
    {
        return ((positiveOrNegative % modulo) + modulo) % modulo;
    }

    UV from(const ZenLib::ZMath::float2& source)
    {
        return UV { source.x, source.y };
    }

    VEC3 from(const ZenLib::ZMath::float3& source)
    {
        return VEC3 { source.x, source.y, source.z };
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
        auto texFilename = texFilepath.filename().u8string();
        ::util::asciiToLowercase(texFilename);
        return texFilename;
    }

    const D3DXCOLOR sampleLightmap(const VEC3& vertexPos, const ARRAY_UV& uvLightmap, const SoftwareLightmapTexture& lightmapTex) {
        // CPU per vertex sampling because the lightmaps are so low resolution that per pixel sampling is just overkill
        // GPU per vertex sampling might be easier, however this is probably faster during rendering.
        
        float uAbs = (lightmapTex.width * uvLightmap.u) + 0.5f;
        float vAbs = (lightmapTex.height * uvLightmap.v) + 0.5f;
        uint32_t uPixel = wrapModulo(uAbs, lightmapTex.width);
        uint32_t vPixel = wrapModulo(vAbs, lightmapTex.height);

        const uint32_t pixelSize = 4;
        const auto& image = *lightmapTex.image;

        uint8_t* pixel = image.pixels + (image.rowPitch * vPixel) + (pixelSize * uPixel);

        D3DXCOLOR result;
        result.r = (*(pixel + 0)) / 255.0f;
        result.g = (*(pixel + 1)) / 255.0f;
        result.b = (*(pixel + 2)) / 255.0f;
        result.a = (*(pixel + 3)) / 255.0f;
        return result;
    }

    void loadWorldMesh(
        unordered_map<Material, vector<WORLD_VERTEX>>& target,
        ZenLoad::zCMesh& worldMesh,
        vector<SoftwareLightmapTexture>& lightmapTextures)
    {
        ZenLoad::PackedMesh packedMesh;
        worldMesh.packMesh(packedMesh, 0.01f);

        for (const auto& submesh : packedMesh.subMeshes) {
            if (submesh.indices.empty()) {
                continue;
            }
            if (submesh.triangleLightmapIndices.empty()) {
                throw std::logic_error("Expected world mesh to have lighmap information!");
            }

            vector<WORLD_VERTEX> faces;
            unordered_map<uint32_t, bool> lightmaps;// values unused

            int32_t faceIndex = 0;
            for (int32_t indicesIndex = 0; indicesIndex < submesh.indices.size(); indicesIndex += 3) {

                const array zenFace = getFaceVerts(packedMesh.vertices, submesh.indices, indicesIndex);

                ZenLoad::Lightmap lightmap;
                const int16_t faceLightmapIndex = submesh.triangleLightmapIndices[faceIndex];
                if (faceLightmapIndex != -1) {
                    lightmap = worldMesh.getLightmapReferences()[faceLightmapIndex];
                    lightmaps.insert({ lightmap.lightmapTextureIndex , true });
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
                        vertex.colorLightmap = { 1, 1, 1, 1 };
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
                        // TODO does renderer even need lightmap index? we already sample it below!

                        const auto& lightmapTex = lightmapTextures[lightmap.lightmapTextureIndex];
                        vertex.colorLightmap = sampleLightmap(vertex.pos, vertex.uvLightmap, lightmapTex);
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

	void loadInstanceMesh(unordered_map<Material, vector<POS_NORMAL_UV>>& target, ZenLoad::zCProgMeshProto& mesh)
    {
        ZenLoad::PackedMesh packedMesh;
        mesh.packMesh(packedMesh, 0.01f);

        for (const auto& submesh : packedMesh.subMeshes) {
            if (submesh.indices.empty()) {
                continue;
            }
            if (!submesh.triangleLightmapIndices.empty()) {
                throw std::logic_error("Expected VOB mesh to NOT have lighmap information!");
            }

            vector<POS_NORMAL_UV> faces;
            for (uint32_t indicesIndex = 0; indicesIndex < submesh.indices.size(); indicesIndex += 3) {
                
                const array zenFace = getFaceVerts(packedMesh.vertices, submesh.indices, indicesIndex);
                array<POS_NORMAL_UV, 3> face;

                uint32_t vertexIndex = 0;
                for (const auto& zenVert : zenFace) {
                    POS_NORMAL_UV vertex;
                    vertex.pos = from(zenVert.Position);
                    vertex.normal = from(zenVert.Normal);
                    vertex.uvDiffuse = from(zenVert.TexCoord);

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