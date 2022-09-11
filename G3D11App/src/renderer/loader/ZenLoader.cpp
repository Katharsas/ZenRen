#include "stdafx.h"
#include "ZenLoader.h"

#include <filesystem>

#include "../../Util.h"
#include "../RenderUtil.h"

#include "vdfs/fileIndex.h"
#include "zenload/zCMesh.h"
#include "zenload/zenParser.h"
#include "zenload/ztex2dds.h"

namespace renderer::loader {

    using namespace ZenLib;
    using ::util::asciiToLowercase;
    using ::util::getOrCreate;

    typedef POS_NORMAL_UV VERTEX;

    ZenLoad::ZenParser parser;
    ZenLoad::zCMesh* worldMesh;

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

    std::unordered_map<std::string, std::vector<POS_NORMAL_UV>> loadZen() {
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

        ZenLoad::PackedMesh packedWorldMesh;
        parser.getWorldMesh()->packMesh(packedWorldMesh, 0.01f);

        LOG(INFO) << "Zen loaded!";

        std::unordered_map<std::string, std::vector<VERTEX>> matsToVertices;

        for (const auto& zenSubmesh : packedWorldMesh.subMeshes) {

            std::vector<VERTEX> faces;

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
                        vertex.uv.u = zenVert.TexCoord.x;
                        vertex.uv.v = zenVert.TexCoord.y;
                    }
                    vertices.at(2 - vertexIndex) = vertex;// flip faces apparently, but not z ?!
                    vertexIndex++;
                }
                faces.insert(faces.end(), vertices.begin(), vertices.end());
            }

            const auto& texture = zenSubmesh.material.texture;

            if (!texture.empty()) {

                if (texture == std::string("OWODWAMOUNTAINFARCLOSE.TGA")) {
                    LOG(INFO) << "HEY";
                }

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

    std::vector<ZenLightmapTexture> loadZenLightmaps() {
        std::vector<ZenLightmapTexture> ddsTextures;
        for (auto& lightmap : worldMesh->getLightmapTextures()) {
            int32_t width;
            int32_t height;
            std::vector<uint8_t> ddsRaw;
            int32_t message = ZenLoad::convertZTEX2DDS(lightmap, ddsRaw, true, &width, &height);
            if (message != 0) {
                LOG(WARNING) << "Failed to convert lightmap zTex to DDS: Error code '" << message << "'!";
            }
            ZenLightmapTexture info = { width, height, ddsRaw };
            ddsTextures.push_back(info);
        }
        return ddsTextures;
    }
}
