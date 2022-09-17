#include "stdafx.h"
#include "ZenLoader.h"

#include <filesystem>

#include "../../Util.h"
#include "../RenderUtil.h"

#include "DirectXTex.h"
#include "vdfs/fileIndex.h"
#include "zenload/zCMesh.h"
#include "zenload/zenParser.h"
#include "zenload/ztex2dds.h"

namespace renderer::loader {

    using namespace DirectX;
    using namespace ZenLib;
    using ::util::asciiToLowercase;
    using ::util::getOrCreate;

    typedef WORLD_VERTEX VERTEX;

    ZenLoad::ZenParser parser;
    ZenLoad::zCMesh* worldMesh;

    std::vector<ZenLightmapTexture> lightmapTextures;
    std::vector<Image> lightmapTexturesData;

    /**
     * Recursive function to list some data about the zen-file
     */
    void listVobInformation(const std::vector<ZenLoad::zCVobData>& vobs)
    {
        for (const ZenLoad::zCVobData& v : vobs)
        {
            if (!v.visual.empty())
            {
                // More information about what a vob stores can be found in the zTypes.h-File
                LOG(INFO) << "Vob at " << v.position.toString() << ", Visual: " << v.visual;
            }

            // List the information about the children as well
            //listVobInformation(v.childVobs);
        }
    }

    int32_t wrapModulo(int32_t positiveOrNegative, int32_t modulo) {
        return ((positiveOrNegative % modulo) + modulo) % modulo;
    }

    void loadZenLightmaps() {
        for (auto& lightmap : worldMesh->getLightmapTextures()) {
            int32_t width;
            int32_t height;
            std::vector<uint8_t> ddsRaw;
            int32_t message = ZenLoad::convertZTEX2DDS(lightmap, ddsRaw, true, &width, &height);
            if (message != 0) {
                LOG(WARNING) << "Failed to convert lightmap zTex to DDS: Error code '" << message << "'!";
            }
            ZenLightmapTexture tex = { width, height, ddsRaw };
            lightmapTextures.push_back(tex);

            ScratchImage image;
            TexMetadata metadata;
            HRESULT result = DirectX::LoadFromDDSMemory(tex.ddsRaw.data(), tex.ddsRaw.size(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, &metadata, image);

            ScratchImage* bytePerPixelImage = new ScratchImage;// leak
            result = Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DXGI_FORMAT_R8G8B8A8_UNORM, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, *bytePerPixelImage);

            lightmapTexturesData.push_back(*bytePerPixelImage->GetImage(0, 0, 0));
        }
    }

    std::vector<ZenLightmapTexture>& getLightmapTextures() {
        return lightmapTextures;
    }

    std::unordered_map<std::string, std::vector<VERTEX>> loadZen() {
        const std::string vdfsArchiveToLoad = "data_g1/worlds.VDF";
        const std::string zenFilename = "WORLD.ZEN";

        VDFS::FileIndex::initVDFS("Foo");
        VDFS::FileIndex vdf;
        vdf.loadVDF(vdfsArchiveToLoad);
        vdf.finalizeLoad();

        parser = ZenLoad::ZenParser(zenFilename, vdf);
        if (parser.getFileSize() == 0)
        {
            LOG(FATAL) << "ZEN-File either not found or empty!";
        }

        // Since this is a usual level-zen, read the file header
        // You will most likely allways need to do that
        parser.readHeader();

        // Do something with the header, if you want.
        LOG(INFO) << "Zen author: " << parser.getZenHeader().user;
        LOG(INFO) << "Zen date: " << parser.getZenHeader().date;
        LOG(INFO) << "Zen object count: " << parser.getZenHeader().objectCount;

        ZenLoad::oCWorldData world;
        parser.readWorld(world);

        worldMesh = parser.getWorldMesh();

        loadZenLightmaps();

        ZenLoad::PackedMesh packedWorldMesh;
        parser.getWorldMesh()->packMesh(packedWorldMesh, 0.01f);

        LOG(INFO) << "Zen loaded!";

        std::unordered_map<std::string, std::vector<VERTEX>> matsToVertices;

        for (const auto& zenSubmesh : packedWorldMesh.subMeshes) {

            std::vector<VERTEX> faces;
            std::unordered_map<uint32_t, bool> lightmaps;

            int32_t faceIndex = 0;
            for (int32_t indicesIndex = 0; indicesIndex < zenSubmesh.indices.size(); indicesIndex += 3) {
                const std::array<uint32_t, 3> faceIndices = {
                    zenSubmesh.indices[indicesIndex],
                    zenSubmesh.indices[indicesIndex + 1],
                    zenSubmesh.indices[indicesIndex + 2]
                };
                const auto face = {
                    packedWorldMesh.vertices.at(faceIndices[0]),
                    packedWorldMesh.vertices.at(faceIndices[1]),
                    packedWorldMesh.vertices.at(faceIndices[2]),
                };
                
                ZenLoad::Lightmap lightmap;
                const int16_t faceLightmapIndex = zenSubmesh.triangleLightmapIndices[faceIndex];
                if (faceLightmapIndex != -1) {
                    lightmap = worldMesh->getLightmapReferences()[faceLightmapIndex];
                    //worldMesh->getLightmapTextures
                    lightmaps.insert({ lightmap.lightmapTextureIndex , true });
                }

                std::array<VERTEX, 3> vertices;

                uint32_t vertexIndex = 0;
                for (const auto& zenVert : face) {
                    VERTEX vertex;
                    {
                        vertex.pos.x = zenVert.Position.x;
                        vertex.pos.y = zenVert.Position.y;
                        vertex.pos.z = zenVert.Position.z;
                    } {
                        vertex.normal.x = zenVert.Normal.x;
                        vertex.normal.y = zenVert.Normal.y;
                        vertex.normal.z = zenVert.Normal.z;
                    } {
                        vertex.uvDiffuse.u = zenVert.TexCoord.x;
                        vertex.uvDiffuse.v = zenVert.TexCoord.y;
                    } {
                        if (faceLightmapIndex == -1) {
                            vertex.uvLightmap = { 0, 0, -1 };
                            vertex.colorLightmap = { 1, 1, 1, 1 };
                        }
                        else {
                            float unscale = 100;
                            XMVECTOR pos = XMVectorSet(vertex.pos.x * unscale, vertex.pos.y * unscale, vertex.pos.z * unscale, 0);
                            XMVECTOR origin = XMVectorSet(lightmap.origin.x, lightmap.origin.y, lightmap.origin.z, 0);
                            XMVECTOR normalU = XMVectorSet(lightmap.normals[0].x, lightmap.normals[0].y, lightmap.normals[0].z, 0);
                            XMVECTOR normalV = XMVectorSet(lightmap.normals[1].x, lightmap.normals[1].y, lightmap.normals[1].z, 0);
                            XMVECTOR lightmapDir = pos - origin;
                            vertex.uvLightmap.u = XMVectorGetX(XMVector3Dot(lightmapDir, normalU));
                            vertex.uvLightmap.v = XMVectorGetX(XMVector3Dot(lightmapDir, normalV));
                            vertex.uvLightmap.i = lightmap.lightmapTextureIndex;

                            // CPU per vertex sampling because the lightmaps are so low resolution that per pixel sampling is just overkill
                            // GPU per vertex sampling might be easier, however this is probably faster during rendering.
                            const auto& lightmapTex = lightmapTextures[lightmap.lightmapTextureIndex];
                            float uAbs = (lightmapTex.width * vertex.uvLightmap.u) + 0.5f;
                            float vAbs = (lightmapTex.height * vertex.uvLightmap.v) + 0.5f;
                            uint32_t uPixel = wrapModulo(uAbs, lightmapTex.width);
                            uint32_t vPixel = wrapModulo(vAbs, lightmapTex.height);

                            const uint32_t pixelSize = 4;
                            const auto image = lightmapTexturesData[lightmap.lightmapTextureIndex];

                            uint8_t* pixel = image.pixels + (image.rowPitch * vPixel) + (pixelSize * uPixel);
                            vertex.colorLightmap.r = (*(pixel + 0)) / 255.0f;
                            vertex.colorLightmap.g = (*(pixel + 1)) / 255.0f;
                            vertex.colorLightmap.b = (*(pixel + 2)) / 255.0f;
                            vertex.colorLightmap.a = (*(pixel + 3)) / 255.0f;
                        }
                    }
                    vertices.at(2 - vertexIndex) = vertex;// flip faces apparently, but not z ?!
                    vertexIndex++;

                    // lightmap test
                    /*if (indicesIndex == 0) {
                        for (auto& vertex : vertices) {
                            worldMesh->getLightmapReferences()
                        }
                    }*/
                }
                faces.insert(faces.end(), vertices.begin(), vertices.end());
                faceIndex++;
            }

            /*LOG(INFO) << "LIGHTMAPS USED: " << lightmaps.size();
            for (auto& lightmapIt : lightmaps) {
                LOG(INFO) << "    " << lightmapIt.first;
            }*/

            const auto& texture = zenSubmesh.material.texture;

            if (!texture.empty()) {

                /*if (texture == std::string("OWODWAMOUNTAINFARCLOSE.TGA")) {
                    LOG(INFO) << "HEY";
                }*/

                //LOG(INFO) << "Zen material: " << zenSubmesh.material.matName;
                //LOG(INFO) << "Zen texture: " << texture;
                //LOG(INFO) << "Zen vertices: " << faces.size();
                //LOG(INFO) << "";

                auto& texFilepath = std::filesystem::path(texture);
                auto& texFilename = texFilepath.filename().u8string();
                asciiToLowercase(texFilename);

                matsToVertices.insert({ texFilename, faces });
                //auto& matVertices = getOrCreate(matsToVertices, texFilename);
                //matVertices.insert(matVertices.end(), faces.begin(), faces.end());
            }
        }

        //std::string dumpTex = "owodpatrgrassmi.tga";
        //asciiToLowercase(dumpTex);
        //util::dumpVerts(dumpTex, matsToVertices.at(dumpTex));

        //LOG(INFO) << "Listing vobs...";
        //listVobInformation(world.rootVobs);

        return matsToVertices;

        // Print some sample-data for vobs which got a visual
        /*
        

        std::cout << "Listing waypoints..." << std::endl;
        for (const ZenLoad::zCWaypointData& v : world.waynet.waypoints)
        {
            std::cout << "Waypoint [" << v.wpName << "] at " << v.position.toString() << std::endl;
        }

        std::cout << "Exporting obj..." << std::endl;
        Utils::exportPackedMeshToObj(packedWorldMesh, (zenFileName + std::string(".OBJ")));
        */
	}
}
