#include "stdafx.h"
#include "ZenLoader.h"

#include <filesystem>
#include <queue>
#include <stdexcept>
#include <filesystem>

#include "../../Util.h"
#include "../RenderUtil.h"
#include "MeshFromVdfLoader.h"
#include "TexFromVdfLoader.h"
#include "VertPosLookup.h"
#include "StaticLightFromGroundFace.h"

#include "DirectXTex.h"
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
    using ::util::getOrCreate;

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

    bool existsInstanceMesh(string& visualname, VDFS::FileIndex& vdf)
    {
        // .mrm does not exist for worldmesh parts
        string filename = visualname + ".MRM";
        return vdf.hasFile(filename);
    }

    bool loadInstanceMesh(unordered_map<Material, VEC_VERTEX_DATA>& target, string& visualname, VDFS::FileIndex& vdf, XMMATRIX& transform, const D3DXCOLOR& lightStatic)
    {
        string filename = visualname + ".MRM";
        if (!vdf.hasFile(filename)) {
            return false;
        }

        ZenLoad::zCProgMeshProto rawMesh(filename, vdf);

        loadInstanceMesh(target, rawMesh, transform, lightStatic, visualname);
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

    inline std::ostream& operator <<(std::ostream& os, const D3DXCOLOR& that)
    {
        return os << "[R:" << that.r << " G:" << that.g << " B:" << that.b << " A:" << that.a << "]";
    }

    vector<StaticInstance> loadVobs(vector<ZenLoad::zCVobData>& rootVobs, const unordered_map<Material, VEC_VERTEX_DATA>& worldMeshData) {
        vector<StaticInstance> statics;
        vector<ZenLoad::zCVobData*> vobs;
        flattenVobTree(rootVobs, vobs);
        
        int32_t totalDurationMicros = 0;
        int32_t maxDurationMicros = 0;

        const auto spatialCache = createSpatialCache(worldMeshData);

        for (auto vobPtr : vobs) {
            ZenLoad::zCVobData vob = *vobPtr;

            bool isVisible = !vob.visual.empty() && (vob.visual.find(".3DS") != string::npos);
            if (isVisible) {
                auto visualname = vob.visual.substr(0, vob.visual.find_last_of('.'));
                StaticInstance instance;
                instance.meshName = visualname;

                XMVECTOR pos = XMVectorSet(vob.position.x, vob.position.y, vob.position.z, 100.0f);
                pos = pos / 100;
                XMMATRIX rotate = XMMATRIX(vob.rotationMatrix.mv);
                XMMATRIX translate = XMMatrixTranslationFromVector(pos);
                instance.transform = rotate * translate;

                // TODO check if would should receive static light from groundPoly (static flag in vob?)
                // TODO is default static vob color?
                const auto now = std::chrono::high_resolution_clock::now();
                auto colLight = getLightStaticAtPos(pos, worldMeshData, spatialCache);
                if (colLight.has_value()) {
                    instance.colLightStatic = colLight.value();
                }
                else {
                    // TODO set default light values
                    instance.colLightStatic = D3DXCOLOR(0, 0, 0, 1);
                }
                const auto duration = std::chrono::high_resolution_clock::now() - now;
                
                const auto durationMicros = static_cast<int32_t> (duration / std::chrono::microseconds(1));
                totalDurationMicros += durationMicros;
                maxDurationMicros = std::max(maxDurationMicros, durationMicros);
                //LOG(INFO) << "Found VobInstance GroundPoly Color (" << std::to_string(durationMicros) << " micros): " << instance.colLightStatic;
                
                statics.push_back(instance);
            }
        }

        LOG(INFO) << "Vob StaticLight for " << statics.size() << " instances: " << std::to_string(totalDurationMicros/1000) << " ms total, " << std::to_string(maxDurationMicros) << " micros worst instance";

        return statics;
    }

    RenderData loadZen(string& zenFilename, VDFS::FileIndex* vdf) {

        auto parser = ZenLoad::ZenParser(zenFilename, *vdf);
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
        
        ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();

        vector<InMemoryTexFile> lightmaps = loadZenLightmaps(worldMesh);

        unordered_map<Material, VEC_VERTEX_DATA> worldMeshData;
        loadWorldMesh(worldMeshData, parser.getWorldMesh());

        vector<StaticInstance> vobs = loadVobs(world.rootVobs, worldMeshData);

        LOG(INFO) << "Zen loaded!";

        unordered_map<Material, VEC_VERTEX_DATA> staticMeshData;
        for (auto& vob : vobs) {
            auto& visualname = vob.meshName;

            if (existsInstanceMesh(visualname, *vdf)) {
                bool loaded = loadInstanceMesh(staticMeshData, visualname, *vdf, vob.transform, vob.colLightStatic);
            }
            else {
                LOG(DEBUG) << "Skipping VOB " << visualname << " (visual not found)";
            }
        }

        LOG(INFO) << "Meshes loaded!";

        return {
            worldMeshData,
            staticMeshData,
            lightmaps
        };
	}
}
