#include "stdafx.h"
#include "ZenLoader.h"

#include <filesystem>
#include <queue>
#include <stdexcept>
#include <filesystem>

#include "../../Util.h"
#include "../RenderUtil.h"
#include "TextureFinder.h"
#include "MeshFromVdfLoader.h"

#include "DirectXTex.h"
#include "vdfs/fileIndex.h"
#include "zenload/zCMesh.h"
#include "zenload/zenParser.h"
#include "zenload/ztex2dds.h"
#include "zenload/zCProgMeshProto.h"

namespace renderer::loader {

    using namespace DirectX;
    using namespace ZenLib;
    using ::std::string;
    using ::std::array;
    using ::std::vector;
    using ::std::unordered_map;
    //using ::util::asciiToLowercase;
    //using ::util::getOrCreate;

    ZenLoad::ZenParser parser;
    ZenLoad::zCMesh* worldMesh;

    vector<ZenLightmapTexture> lightmapTextures;
    vector<Image> lightmapTexturesData;

    struct StaticInstance {
        std::string visual;
        XMMATRIX transform;
    };

    /**
     * Recursive function to list some data about the zen-file
     */
    void listVobInformation(const vector<ZenLoad::zCVobData>& vobs)
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



    void loadZenLightmaps() {
        for (auto& lightmap : worldMesh->getLightmapTextures()) {
            int32_t width;
            int32_t height;
            vector<uint8_t> ddsRaw;
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

    vector<ZenLightmapTexture>& getLightmapTextures() {
        return lightmapTextures;
    }

    unordered_map<Material, vector<WORLD_VERTEX>> loadWorldMesh()
    {
        unordered_map<Material, vector<WORLD_VERTEX>> matsToVertices;

        vector<SoftwareLightmapTexture> softwareTextures;
        for (int i = 0; i < lightmapTextures.size(); i++) {
            softwareTextures.push_back({
                lightmapTextures[i].width,
                lightmapTextures[i].height,
                &lightmapTexturesData[i],
            });
        }

        loadWorldMesh(matsToVertices, *parser.getWorldMesh(), softwareTextures);
        return matsToVertices;
    }

    bool existsInstanceMesh(string& visualname, VDFS::FileIndex& vdf)
    {
        // .mrm does not exist for worldmesh parts
        string filename = visualname + ".MRM";
        return vdf.hasFile(filename);
    }

    bool loadInstanceMesh(unordered_map<Material, vector<POS_NORMAL_UV>> target, string& visualname, VDFS::FileIndex& vdf)
    {
        string filename = visualname + ".MRM";
        if (!vdf.hasFile(filename)) {
            return false;
        }

        ZenLoad::zCProgMeshProto rawMesh(filename, vdf);

        loadInstanceMesh(target, rawMesh);
        return true;
    }

    void flattenVobTree(vector<ZenLoad::zCVobData>& vobs, vector<ZenLoad::zCVobData*>& target)
    {
        for (ZenLoad::zCVobData& vob : vobs) {
            if (!vob.visual.empty() && vob.visual.find(".3DS") != string::npos) {
                target.push_back(&vob);
            }
            flattenVobTree(vob.childVobs, target);
        }
    }

    std::vector<StaticInstance> loadVobs(std::vector<ZenLoad::zCVobData>& rootVobs) {
        vector<StaticInstance> statics;
        vector<ZenLoad::zCVobData*> vobs;
        flattenVobTree(rootVobs, vobs);
        
        for (auto vobPtr : vobs) {
            ZenLoad::zCVobData vob = *vobPtr;

            bool isVisible = !vob.visual.empty() && (vob.visual.find(".3DS") != string::npos);
            if (isVisible) {
                StaticInstance instance;
                auto visualname = vob.visual.substr(0, vob.visual.find_last_of('.'));
                instance.visual = visualname;
                auto& transform = vob.worldMatrix;

                instance.transform = XMMatrixIdentity();
                if (sizeof(instance.transform) != sizeof(transform)) {
                    throw std::logic_error("Failed to convert ZenLib::ZMath::Matrix to DirectX::XMMATRIX");
                }
                memcpy(&transform, &instance.transform, sizeof(instance.transform));

                statics.push_back(instance);
            }
        }
        return statics;
    }

    unordered_map<Material, std::vector<WORLD_VERTEX>> loadZen() {
        const string vdfsArchives = "data_g1";
        const string zenFilename = "WORLD.ZEN";

        VDFS::FileIndex::initVDFS("Foo");
        VDFS::FileIndex vdf;

        for (const auto& entry : std::filesystem::directory_iterator(vdfsArchives)) {
            vdf.loadVDF(entry.path().string());
        }
        //std::cout << entry.path() << std::endl;

        
        //vdf.loadVDF("data_g1/anims.VDF");
        vdf.finalizeLoad();

        /*
        LOG(INFO) << "--------------------------";
        auto files = vdf.getKnownFiles();
        for (auto file : files) {
            if (::util::startsWith(file, "OW_O_BIGBRIDGEMIDDLE")) {
                LOG(INFO) << file;
            }
            if (::util::endsWith(file, "MRM")) {
                LOG(INFO) << file;
            }
        }
        */

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
        auto matsToVertices = loadWorldMesh();
        auto vobs = loadVobs(world.rootVobs);

        unordered_map<string, unordered_map<Material, vector<POS_NORMAL_UV>>> meshes;
        for (auto& vob : vobs) {
            auto visualname = vob.visual;

            // if we have not extracted visual for this vob yet
            auto it = meshes.find(visualname);
            if (it == meshes.end())
            {
                unordered_map<Material, vector<POS_NORMAL_UV>> meshData;
                bool loaded = loadInstanceMesh(meshData, visualname, vdf);
                if (loaded && !meshData.empty()) {
                    meshes.insert({ visualname, meshData });
                }
            }
        }

        LOG(INFO) << "Zen loaded!";

        return matsToVertices;
	}
}
