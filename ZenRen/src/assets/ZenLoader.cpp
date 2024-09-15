#include "stdafx.h"
#include "ZenLoader.h"

#include <filesystem>
#include <queue>
#include <stdexcept>
#include <filesystem>

#include "../Util.h"
#include "render/RenderUtil.h"
#include "render/MeshUtil.h"
#include "AssetCache.h"
#include "MeshFromVdfLoader.h"
#include "DebugMeshes.h"
#include "TexFromVdfLoader.h"
#include "LookupTrees.h"
#include "FindGroundFace.h"
#include "StaticLightFromVobLights.h"

#include "DirectXTex.h"
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
    bool debugTintVobStaticLight = false;
    bool debugStaticLights = false;
    bool debugStaticLightRays = false;

    namespace FormatsSource
    {
        const FileExt __3DS = FileExt::create(".3DS");
        const FileExt ASC = FileExt::create(".ASC");
        const FileExt MDS = FileExt::create(".MDS");
        const FileExt PFX = FileExt::create(".PFX");
        const FileExt MMS = FileExt::create(".MMS");
        const FileExt TGA = FileExt::create(".TGA");
    }

    // see https://github.com/Try/OpenGothic/wiki/Gothic-file-formats
    // MRM = single mesh (multiresolution)
    // MDM = single mesh
    // MDH = model hierarchy
    // MDL = model hierarchy including all used single meshes
    namespace FormatsCompiled
    {
        const FileExt MRM = FileExt::create(".MRM");
        const FileExt MDM = FileExt::create(".MDM");
        const FileExt MDH = FileExt::create(".MDH");
        const FileExt MDL = FileExt::create(".MDL");
    }


    XMVECTOR bboxCenter(const array<VEC3, 2>& bbox)
    {
        return 0.5f * (toXM4Pos(bbox[0]) + toXM4Pos(bbox[1]));
    }

    std::optional<string> getVobVisual(VDFS::FileIndex& vdf, const string& visual)
    {
        using namespace FormatsSource;
        using namespace FormatsCompiled;

        auto visualNoExt = visual.substr(0, visual.find_last_of('.'));
        string meshFile;

        if (__3DS.isExtOf(visual)) {
            meshFile = visualNoExt + MRM.str();
            // TODO should probably fallback to MDH if missing
        }
        else if (ASC.isExtOf(visual)) {
            // TODO do MRM files exist? do MDH files exist so we can re-use single meshes instead of reparsing them from MDL all the time?
            // if a single mesh is shared between multiple hierarchies, does Gothic still put instances of them into MDL files?
            meshFile = visualNoExt + MDL.str();
        }
        else if (MDS.isExtOf(visual)) {
            // in this case, both MRM/MDM or MDL might exist
            meshFile = visualNoExt + MDL.str();
            if (!vdf.hasFile(meshFile)) {
                meshFile = visualNoExt + MDM.str();
            }
        }
        if (vdf.hasFile(meshFile)) {
            return meshFile;
        }
        else {
            return std::nullopt;
        }
    }

    bool loadInstanceMesh(VERT_CHUNKS_BY_MAT& target, VDFS::FileIndex& vdf, const StaticInstance& instance)
    {
        using namespace FormatsCompiled;

        const auto meshNameOpt = getVobVisual(vdf, instance.meshName);
        if (meshNameOpt.has_value()) {
            const auto& meshName = meshNameOpt.value();
            
            // load meshes
            if (endsWithEither(meshName, { MRM, MDM })) {
                const auto& mesh = getOrParseMesh(vdf, meshName, true);
                loadInstanceMesh(target, mesh.mesh, mesh.packed, instance);
            }
            else if (endsWithEither(meshName, { MDL })) {
                const auto& meshLib = getOrParseMeshLib(vdf, meshName, true);
                loadInstanceMeshLib(target, meshLib, instance);
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
                auto scale = toVec3(0.7f * XMVectorAbs(toXM4Pos(instance.bbox[1]) - toXM4Pos(instance.bbox[0])));
                loadPointDebugVisual(target, center, scale, D3DXCOLOR(0, 0, 1, 1));
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

    inline std::ostream& operator <<(std::ostream& os, const D3DXCOLOR& that)
    {
        return os << "[R:" << that.r << " G:" << that.g << " B:" << that.b << " A:" << that.a << "]";
    }

    array<VEC3, 2> bboxToVec3Scaled(const ZMath::float3 (& bbox)[2])
    {
        return {
            from(bbox[0], G_ASSET_RESCALE),
            from(bbox[1], G_ASSET_RESCALE)
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
                fromSRGB(D3DXCOLOR(vob.zCVobLight.color)),
                vob.zCVobLight.range * G_ASSET_RESCALE,
            };

            lights.push_back(light);
        }

        LOG(INFO) << "VOBs: Loaded " << lights.size() << " static lights.";
        return lights;
    }

    D3DXCOLOR interpolateColor(const VEC3& pos, const VERT_CHUNKS_BY_MAT& meshData, const VertKey& vertKey)
    {
        auto& vecVertData = vertKey.get(meshData);
        auto& vecPos = vecVertData.vecPos;
        auto& others = vecVertData.vecOther;
        auto vertIndex = vertKey.vertIndex;
        float v0Distance = std::sqrt(std::pow(vecPos[vertIndex + 0].x - pos.x, 2.f) + std::pow(vecPos[vertIndex + 0].z - pos.z, 2.f));
        float v1Distance = std::sqrt(std::pow(vecPos[vertIndex + 1].x - pos.x, 2.f) + std::pow(vecPos[vertIndex + 1].z - pos.z, 2.f));
        float v2Distance = std::sqrt(std::pow(vecPos[vertIndex + 2].x - pos.x, 2.f) + std::pow(vecPos[vertIndex + 2].z - pos.z, 2.f));
        float totalDistance = v0Distance + v1Distance + v2Distance;
        float v0Contrib = 1 - (v0Distance / totalDistance);
        float v1Contrib = 1 - (v1Distance / totalDistance);
        float v2Contrib = 1 - (v2Distance / totalDistance);
        D3DXCOLOR v0Color = others[vertIndex + 0].colLight;
        D3DXCOLOR v1Color = others[vertIndex + 1].colLight;
        D3DXCOLOR v2Color = others[vertIndex + 2].colLight;
        D3DXCOLOR colorAverage = ((v0Color * v0Contrib) + (v1Color * v1Contrib) + (v2Color * v2Contrib)) / 2.f;
        return colorAverage;
    }

    vector<StaticInstance> loadVobs(
        vector<ZenLoad::zCVobData>& rootVobs,
        const VERT_CHUNKS_BY_MAT& worldMeshData,
        const vector<Light>& lightsStatic,
        const bool isOutdoorLevel)
    {
        vector<StaticInstance> statics;
        vector<const ZenLoad::zCVobData*> vobs;
        getVobsWithVisuals(rootVobs, vobs);
        
        int32_t resolvedStaticLight = 0;

        int32_t totalDurationMicros = 0;
        int32_t maxDurationMicros = 0;

        const auto worldFaceLookup = createVertLookup(worldMeshData);
        const auto lightStaticLookup = createLightLookup(lightsStatic);

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
            instance.bbox = bboxToVec3Scaled(vob.bbox);
            {
                const auto now = std::chrono::high_resolution_clock::now();

                instance.dirLightStatic = -1 * XMVectorSet(1, -0.5, -1.0, 0);

                D3DXCOLOR colLight;
                const XMVECTOR center = bboxCenter(instance.bbox);
                const std::optional<VertKey> vertKey = getGroundFaceAtPos(center, worldMeshData, worldFaceLookup);

                bool hasLightmap = false;
                if (!isOutdoorLevel) {
                    hasLightmap = true;
                }
                else if (vertKey.has_value()) {
                    const auto& other = vertKey.value().getOther(worldMeshData);
                    if (other[0].uvLightmap.i != -1) {
                        hasLightmap = true;
                    }
                }
                instance.receiveLightSun = !hasLightmap;

                if (hasLightmap) {
                    // TODO
                    // The more lights get summed, the bigger the SRGB summing error is. 
                    // More light additions -> brighter in SRGB than linar; less lights -> closer brightness
                    // The final weight (0.71f) in Vanilla Gothic might be there to counteract this error, but should lead to objects
                    // hit by less objects to be overly dark. Maybe adjust weight to lower value? Check low hit objects.
                        
                    auto optLight = getLightAtPos(bboxCenter(instance.bbox), lightsStatic, lightStaticLookup, worldMeshData, worldFaceLookup);
                    if (optLight.has_value()) {
                        colLight = optLight.value().color;
                        colLight = multiplyColor(colLight, fromSRGB(0.85f));

                        instance.dirLightStatic = optLight.value().dirInverted;
                    }
                    else {
                        colLight = D3DXCOLOR(0, 0, 0, 1);// no lights reached this vob, so its black
                        if (debugStaticLightRays) {
                            colLight = D3DXCOLOR(0, 1, 0, 1);
                        }
                    }
                    resolvedStaticLight++;
                    colLight.a = 0;// indicates that this VOB receives no sky light
                }
                else {
                    if (vertKey.has_value()) {
                        colLight = interpolateColor(toVec3(center), worldMeshData, vertKey.value());
                        resolvedStaticLight++;

                        // outdoor vobs get additional fixed ambient
                        if (isOutdoorLevel) {
                            // TODO
                            // Original game knows if a ground poly is in a portal room (without lightmap, like a cave) or actually outdoors
                            // from per-ground-face flag. Find out how this is calculated or shoot some rays towards the sky.
                            bool isVobIndoor = false;

                            float minLight = isVobIndoor ? fromSRGB(0.2f) : fromSRGB(0.5f);
                            colLight = colLight * (1.f - minLight) + greyscale(minLight);
                            // currently this results in light values between between 0.22 and 0.99 
                        }
                    }
                    else {
                        colLight = fromSRGB(D3DXCOLOR(0.63f, 0.63f, 0.63f, 1));// fallback lightness of (160, 160, 160)
                    }
                    colLight.a = 1;// indicates that this VOB receives full sky light
                }

                if (debugTintVobStaticLight) {
                    colLight = D3DXCOLOR((colLight.r / 3.f) * 2.f, colLight.g, colLight.b, colLight.a);
                }

                instance.colLightStatic = colLight;

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

    void loadZen(render::RenderData& out, string& zenFilename, VDFS::FileIndex* vdf)
    {
        const auto now = std::chrono::high_resolution_clock::now();

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

        loadWorldMesh(out.worldMesh, parser.getWorldMesh());

        LOG(INFO) << "Zen parsed!";

        vector<Light> lightsStatic = loadLights(world.rootVobs);

        auto props = world.properties;
        bool isOutdoorLevel = world.bspTree.mode == ZenLoad::zCBspTreeData::TreeMode::Outdoor;
        vector<StaticInstance> vobs;
        if (loadStaticMeshes) {
            vobs = loadVobs(world.rootVobs, out.worldMesh, lightsStatic, isOutdoorLevel);
            LOG(INFO) << "VOBs loaded!";
        }
        else {
            LOG(INFO) << "VOB loading disabled!";
        }

        VERT_CHUNKS_BY_MAT staticMeshData;
        for (auto& vob : vobs) {
            auto& visualname = vob.meshName;

            bool loaded = loadInstanceMesh(out.staticMeshes, *vdf, vob);
        }

        if (debugStaticLights) {
            for (auto& light : lightsStatic) {
                float scale = light.range / 10.f;
                loadPointDebugVisual(out.staticMeshes, light.pos, { scale, scale, scale });
            }
        }
        if (debugStaticLightRays) {
            for (auto& ray : debugLightToVobRays) {
                loadLineDebugVisual(out.staticMeshes, ray.posStart, ray.posEnd, ray.color);
            }
        }

        //VERTEX_DATA_BY_MAT dynamicMeshData;

        LOG(INFO) << "Meshes loaded!";

        const auto duration = std::chrono::high_resolution_clock::now() - now;
        LOG(INFO) << "Loading finished in: " << duration / std::chrono::milliseconds(1) << " ms.";

        out.isOutdoorLevel = isOutdoorLevel;
        out.worldMeshLightmaps = lightmaps;
	}
}
