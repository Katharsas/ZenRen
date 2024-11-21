#include "stdafx.h"
#include "ZenLoader.h"

#include <filesystem>
#include <queue>
#include <stdexcept>
#include <filesystem>

#include "../render/PerfStats.h"
#include "../Util.h"
#include "render/RenderUtil.h"
#include "render/MeshUtil.h"
#include "AssetCache.h"
#include "MeshFromVdfLoader.h"
#include "DebugMeshes.h"
#include "TexFromVdfLoader.h"
#include "LookupTrees.h"
#include "StaticLightFromVobLights.h"
#include "VobLoader.h"

#include "vdfs/fileIndex.h"
#include "zenload/zCMesh.h"
#include "zenload/zenParser.h"
#include "zenload/ztex2dds.h"
#include "zenload/zCProgMeshProto.h"
#include "zenload/zCModelMeshLib.h"

namespace assets
{
    using namespace render;
    using namespace DirectX;
    using namespace ZenLib;
    using ::std::string;
    using ::std::array;
    using ::std::vector;
    using ::std::unordered_map;
    using ::util::endsWithEither;
    using ::util::getOrCreate;
    using ::util::endsWith;
    using ::util::FileExt;

    bool loadStaticMeshes = true;

    bool debugInstanceMeshBbox = false;
    bool debugInstanceMeshBboxCenter = false;

    bool debugMeshLoading = false;


    bool loadInstanceMesh(VERT_CHUNKS_BY_MAT& target, const Vfs& vfs, const StaticInstance& instance)
    {
        using namespace FormatsCompiled;

        const auto meshNameOpt = getVobVisual(instance.meshName);
        if (meshNameOpt.has_value()) {
            const auto& meshName = meshNameOpt.value();
            
            // load meshes
            if (endsWithEither(meshName, { MRM, MDM })) {
                const auto& mesh = getOrParseMesh(vfs, meshName);
                loadInstanceMesh(target, mesh.mesh, instance, debugMeshLoading);
            }
            else if (endsWithEither(meshName, { MDL })) {
                const auto& meshLib = getOrParseMeshLib(vfs, meshName);
                loadInstanceMeshLib(target, meshLib.meshLib, instance, debugMeshLoading);
            }
            else {
                LOG(DEBUG) << "Visual type not supported! Skipping VOB " << meshName;
                return false;
            }

            // load debug visuals
            if (debugInstanceMeshBbox) {
                loadInstanceMeshBboxDebugVisual(target, instance);
            }
            if (debugInstanceMeshBboxCenter) {
                auto center = toVec3(bboxCenter(instance.bbox));
                auto scale = toVec3(0.7f * XMVectorAbs(instance.bbox[1] - instance.bbox[0]));
                loadPointDebugVisual(target, center, scale, COLOR(0, 0, 1, 1));
            }

            return true;
        }
        else {
            LOG(DEBUG) << "Visual not found: Skipping VOB " << instance.meshName;
            return false;
        }
    }

    void flattenVobTree(const vector<ZenLoad::zCVobData>& vobs, vector<const ZenLoad::zCVobData*>& target, std::function<bool(const ZenLoad::zCVobData& vob)> filter)
    {
        for (const ZenLoad::zCVobData& vob : vobs) {
            if (filter(vob)) {
                target.push_back(&vob);
            }
            flattenVobTree(vob.childVobs, target, filter);
        }
    }

    void getVobsWithVisuals(const vector<ZenLoad::zCVobData>& vobs, vector<const ZenLoad::zCVobData*>& target)
    {
        using namespace FormatsSource;

        const auto filter = [&](const ZenLoad::zCVobData& vob) {

            if (vob.showVisual && !vob.visual.empty()) {
                if (endsWithEither(vob.visual, { __3DS, ASC, MDS })) {
                    return true;
                }
                if (endsWithEither(vob.visual, { PFX, MMS, TGA })) {
                    // TODO effects, morphmeshes, decals
                }
                else {
                    LOG(INFO) << "Visual not supported: " << vob.visual;
                }
            }
            return false;
        };

        return flattenVobTree(vobs, target, filter);
    }

    void getVobStaticLights(const vector<ZenLoad::zCVobData>& vobs, vector<const ZenLoad::zCVobData*>& target)
    {
        const auto filter = [&](const ZenLoad::zCVobData& vob) {
            return vob.vobType == ZenLoad::zCVobData::EVobType::VT_zCVobLight && vob.zCVobLight.lightStatic;
        };
        return flattenVobTree(vobs, target, filter);
    }

    array<XMVECTOR, 2> bboxToXmScaled(const ZMath::float3 (& bbox)[2])
    {
        return {
            toXM4Pos(bbox[0]) * G_ASSET_RESCALE,
            toXM4Pos(bbox[1]) * G_ASSET_RESCALE
        };
    }

    vector<Light> loadLights(const vector<ZenLoad::zCVobData>& rootVobs)
    {
        vector<Light> lights;
        vector<const ZenLoad::zCVobData*> vobs;
        getVobStaticLights(rootVobs, vobs);

        for (auto vobPtr : vobs) {
            ZenLoad::zCVobData vob = *vobPtr;
            Light light = {
                toVec3(toXM4Pos(vob.position) * G_ASSET_RESCALE),
                vob.zCVobLight.lightStatic,
                fromSRGB(COLOR(vob.zCVobLight.color)),
                vob.zCVobLight.range * G_ASSET_RESCALE,
            };

            lights.push_back(light);
        }

        LOG(INFO) << "VOBs: Loaded " << lights.size() << " static lights.";
        return lights;
    }

    vector<StaticInstance> loadVobs(
        vector<ZenLoad::zCVobData>& rootVobs,
        const VERT_CHUNKS_BY_MAT& worldMeshData,
        const vector<Light>& lightsStatic,
        const bool isOutdoorLevel,
        LoadDebugFlags debug)
    {
        vector<StaticInstance> statics;
        vector<const ZenLoad::zCVobData*> vobs;
        getVobsWithVisuals(rootVobs, vobs);
        
        int32_t resolvedStaticLight = 0;

        int32_t totalDurationMicros = 0;
        int32_t maxDurationMicros = 0;

        const FaceLookupContext worldMeshContext = { createVertLookup(worldMeshData), worldMeshData };
        const LightLookupContext lightsStaticContext = { createLightLookup(lightsStatic), lightsStatic };

        for (auto vobPtr : vobs) {
            ZenLoad::zCVobData vob = *vobPtr;
            StaticInstance instance;
            instance.meshName = vob.visual;

            {
                XMVECTOR pos = XMVectorSet(vob.position.x, vob.position.y, vob.position.z, 1.f / G_ASSET_RESCALE);
                pos = pos * G_ASSET_RESCALE;
                XMMATRIX rotate = XMMATRIX(vob.rotationMatrix.mv);
                XMMATRIX translate = XMMatrixTranslationFromVector(pos);
                instance.transform = rotate * translate;
            }

            // VOB instance bounding boxes:
            // There are two types of bbs: Axis-Aligned (AABB) and non-axix-aligned.
            // - storing AABBs requires only 2 vectors ((POS, SIZE) or (POS_MIN, POS_MAX)), non-AABBs more
            // - most acceleration structures (spatial trees) require AABBs
            // - transforming AABBs makes them potentially non-axis-aligned, so this is not trivial; instead we could
            //   - create a new AABB over the transformed BB, but it would be bigger than the original (not optimal) or
            //   - recompute a new optimal AABB from transformed mesh
            // Luckily it seems like the VOB instance BB is already transformed and axis-aligned.
            instance.bbox = bboxToXmScaled(vob.bbox);
            {
                const auto now = std::chrono::high_resolution_clock::now();

                VobLighting lighting = calculateStaticVobLighting(instance.bbox, worldMeshContext, lightsStaticContext, isOutdoorLevel, debug);

                if (debug.vobsTint) {
                    lighting.color = COLOR((lighting.color.r / 3.f) * 2.f, lighting.color.g, lighting.color.b, lighting.color.a);
                }

                instance.lighting = lighting;

                const auto duration = std::chrono::high_resolution_clock::now() - now;
                const auto durationMicros = static_cast<int32_t> (duration / std::chrono::microseconds(1));
                totalDurationMicros += durationMicros;
                maxDurationMicros = std::max(maxDurationMicros, durationMicros);
                //LOG(INFO) << "Found VobInstance GroundPoly Color (" << std::to_string(durationMicros) << " micros): " << instance.colLightStatic;
            }
            statics.push_back(instance);
        }

        LOG(INFO) << "VOBs: Loaded " << statics.size() << " instances:";
        LOG(INFO) << "    " << "Static light: Resolved for " << resolvedStaticLight << " instances.";
        LOG(INFO) << "    " << "Static light: Duration: " << std::to_string(totalDurationMicros / 1000) << " ms total (" << std::to_string(maxDurationMicros) << " micros worst instance)";
        LOG(INFO) << "    " << "Static light: Checked VobLight visibility " << vobLightWorldIntersectChecks << " times.";

        return statics;
    }

    void loadZen(render::RenderData& out, const FileHandle& file, const Vfs& vfs, LoadDebugFlags debug)
    {
        LOG(INFO) << "Loading World!";

        auto samplerTotal = render::stats::TimeSampler();
        samplerTotal.start();
        auto sampler = render::stats::TimeSampler();
        sampler.start();

        auto fileData = assets::getData(file);
        auto parser = ZenLoad::ZenParser((uint8_t*)fileData.data, fileData.size);
        if (parser.getFileSize() == 0)
        {
            LOG(FATAL) << "ZEN-File either not found or empty!";
        }

        // Since this is a usual level-zen, read the file header
        // You will most likely allways need to do that
        parser.readHeader();

        // Do something with the header, if you want.
        LOG(INFO) << "    Zen author: " << parser.getZenHeader().user;
        LOG(INFO) << "    Zen date: " << parser.getZenHeader().date;
        LOG(INFO) << "    Zen object count: " << parser.getZenHeader().objectCount;

        ZenLoad::oCWorldData world;
        parser.readWorld(world);
        ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
        sampler.logMillisAndRestart("Loader: World data parsed");
       

        loadWorldMesh(out.worldMesh, parser.getWorldMesh(), debugMeshLoading);
        sampler.logMillisAndRestart("Loader: World mesh loaded");

        vector<InMemoryTexFile> lightmaps = loadZenLightmaps(worldMesh);
        sampler.logMillisAndRestart("Loader: World lightmaps loaded");

        vector<Light> lightsStatic = loadLights(world.rootVobs);

        auto props = world.properties;
        bool isOutdoorLevel = world.bspTree.mode == ZenLoad::zCBspTreeData::TreeMode::Outdoor;
        vector<StaticInstance> vobs;
        if (loadStaticMeshes) {
            vobs = loadVobs(world.rootVobs, out.worldMesh, lightsStatic, isOutdoorLevel, debug);
            LOG(INFO) << "  VOBs loaded!";
        }
        else {
            LOG(INFO) << "  VOB loading disabled!";
        }
        sampler.logMillisAndRestart("Loader: World VOB data loaded");

        VERT_CHUNKS_BY_MAT staticMeshData;
        for (auto& vob : vobs) {
            auto& visualname = vob.meshName;
            bool loaded = loadInstanceMesh(out.staticMeshes, vfs, vob);
        }
        sampler.logMillisAndRestart("Loader: World VOB meshes loaded");

        if (debug.staticLights) {
            for (auto& light : lightsStatic) {
                float scale = light.range / 10.f;
                loadPointDebugVisual(out.staticMeshes, light.pos, { scale, scale, scale });
            }
        }
        if (debug.staticLightRays) {
            for (auto& ray : debugLightToVobRays) {
                loadLineDebugVisual(out.staticMeshes, ray.posStart, ray.posEnd, ray.color);
            }
        }

        sampler.logMillisAndRestart("Loader: World debug meshes loaded");
        samplerTotal.logMillisAndRestart("World loaded in");

        out.isOutdoorLevel = isOutdoorLevel;
        out.worldMeshLightmaps = lightmaps;
	}
}
