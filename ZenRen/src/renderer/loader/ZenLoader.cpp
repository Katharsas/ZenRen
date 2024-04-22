#include "stdafx.h"
#include "ZenLoader.h"

#include <filesystem>
#include <queue>
#include <stdexcept>
#include <filesystem>

#include "../../Util.h"
#include "../RenderUtil.h"
#include "MeshUtil.h"
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

namespace renderer::loader {

    using namespace DirectX;
    using namespace ZenLib;
    using ::std::string;
    using ::std::array;
    using ::std::vector;
    using ::std::unordered_map;
    //using ::util::asciiToLowercase;
    using ::util::getOrCreate;

    bool debugInstanceMeshBbox = false;
    bool debugInstanceMeshBboxCenter = false;
    bool debugTintVobStaticLight = false;
    bool debugStaticLights = false;
    bool debugStaticLightRays = false;

    XMVECTOR bboxCenter(const array<VEC3, 2>& bbox)
    {
        return 0.5f * (toXM4Pos(bbox[0]) + toXM4Pos(bbox[1]));
    }

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

    bool loadInstanceMesh(unordered_map<Material, VEC_VERTEX_DATA>& target, VDFS::FileIndex& vdf, const StaticInstance& instance)
    {
        string filename = instance.meshName + ".MRM";
        if (!vdf.hasFile(filename)) {
            return false;
        }

        ZenLoad::zCProgMeshProto rawMesh(filename, vdf);

        loadInstanceMesh(target, rawMesh, instance);
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
        const auto filter = [&](const ZenLoad::zCVobData& vob) { return !vob.visual.empty() && vob.visual.find(".3DS") != string::npos; };
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
            from(bbox[0], 0.01f),
            from(bbox[1], 0.01f)
        };
    }

    void multiplyColor(D3DXCOLOR color, const float factor)
    {
        color.r *= factor;
        color.g *= factor;
        color.b *= factor;
    }

    vector<Light> loadLights(const vector<ZenLoad::zCVobData>& rootVobs)
    {
        vector<Light> lights;
        vector<const ZenLoad::zCVobData*> vobs;
        getVobStaticLights(rootVobs, vobs);

        for (auto vobPtr : vobs) {
            ZenLoad::zCVobData vob = *vobPtr;
            Light light = {
                toVec3(toXM4Pos(vob.position) * 0.01f),
                vob.zCVobLight.lightStatic,
                fromSRGB(D3DXCOLOR(vob.zCVobLight.color)),
                vob.zCVobLight.range * 0.01f,
            };

            lights.push_back(light);
        }

        LOG(INFO) << "VOBs: Loaded " << lights.size() << " static lights.";
        return lights;
    }

    D3DXCOLOR interpolateColor(const VEC3& pos, const unordered_map<Material, VEC_VERTEX_DATA>& meshData, const VertKey& vertKey)
    {
        auto& vecVertData = meshData.find(*vertKey.mat)->second;
        auto& vecPos = vecVertData.vecPos;
        auto& others = vecVertData.vecNormalUv;
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

    vector<StaticInstance> loadVobs(vector<ZenLoad::zCVobData>& rootVobs, const unordered_map<Material, VEC_VERTEX_DATA>& worldMeshData, const vector<Light>& lightsStatic, const bool isOutdoorLevel)
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

            bool isVisible = !vob.visual.empty() && (vob.visual.find(".3DS") != string::npos);
            if (isVisible) {
                auto visualname = vob.visual.substr(0, vob.visual.find_last_of('.'));
                StaticInstance instance;
                instance.meshName = visualname;

                {
                    XMVECTOR pos = XMVectorSet(vob.position.x, vob.position.y, vob.position.z, 100.0f);
                    pos = pos / 100;
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
                            //multiplyColor(light.color, fromSRGB(0.71f));
                            multiplyColor(colLight, fromSRGB(0.85f));

                            instance.dirLightStatic = optLight.value().dirInverted;
                        }
                        else {
                            colLight = D3DXCOLOR(0, 0, 0, 1);// no lights reached this vob, so its black
                            if (debugStaticLightRays) {
                                colLight = D3DXCOLOR(0, 1, 0, 1);
                            }
                        }
                        resolvedStaticLight++;
                    }
                    else {
                        if (vertKey.has_value()) {
                            colLight = interpolateColor(toVec3(center), worldMeshData, vertKey.value());
                            resolvedStaticLight++;
                        }
                        else {
                            colLight = fromSRGB(D3DXCOLOR(0.63f, 0.63f, 0.63f, 1));// fallback lightness of (160, 160, 160)
                        }
                    }

                    // outdoor vobs get additional fixed ambient
                    if (isOutdoorLevel && !hasLightmap) {
                        // TODO
                        // Original game knows if a ground poly is in a portal room (without lightmap, like a cave) or actually outdoors
                        // from per-ground-face flag. Find out how this is calculated or shoot some rays towards the sky.
                        bool isVobIndoor = false;
                        if (isVobIndoor) {
                            colLight = colLight * fromSRGB(0.8f) + fromSRGB(greyscale(0.2f));
                            colLight = (colLight * 1.4f) + greyscale(0.05f);// simulate effect of SRGB addition error
                        }
                        else {
                            colLight = colLight * fromSRGB(0.5f) + fromSRGB(greyscale(0.5f));
                            colLight = (colLight * 2.5f) - greyscale(0.25f);// simulate effect of SRGB addition error
                        }
                    }

                    if (debugTintVobStaticLight) {
                        colLight = D3DXCOLOR((colLight.r / 3.f) * 2.f, colLight.g, colLight.b, colLight.a);
                    }

                    colLight.a = 1;
                    instance.colLightStatic = colLight;

                    const auto duration = std::chrono::high_resolution_clock::now() - now;
                    const auto durationMicros = static_cast<int32_t> (duration / std::chrono::microseconds(1));
                    totalDurationMicros += durationMicros;
                    maxDurationMicros = std::max(maxDurationMicros, durationMicros);
                    //LOG(INFO) << "Found VobInstance GroundPoly Color (" << std::to_string(durationMicros) << " micros): " << instance.colLightStatic;
                }
                statics.push_back(instance);
            }
        }

        LOG(INFO) << "VOBs: Loaded " << statics.size() << " instances:";
        LOG(INFO) << "    " << "Static light: Resolved for " << resolvedStaticLight << " instances.";
        LOG(INFO) << "    " << "Static light: Duration: " << std::to_string(totalDurationMicros / 1000) << " ms total (" << std::to_string(maxDurationMicros) << " micros worst instance)";
        LOG(INFO) << "    " << "Static light: Checked VobLight visibility " << vobLightWorldIntersectChecks << " times.";

        return statics;
    }

    RenderData loadZen(string& zenFilename, VDFS::FileIndex* vdf)
    {

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

        vector<Light> lightsStatic = loadLights(world.rootVobs);

        auto props = world.properties;
        bool isOutdoorLevel = world.bspTree.mode == ZenLoad::zCBspTreeData::TreeMode::Outdoor;
        vector<StaticInstance> vobs = loadVobs(world.rootVobs, worldMeshData, lightsStatic, isOutdoorLevel);

        LOG(INFO) << "Zen loaded!";

        unordered_map<Material, VEC_VERTEX_DATA> staticMeshData;
        for (auto& vob : vobs) {
            auto& visualname = vob.meshName;

            if (existsInstanceMesh(visualname, *vdf)) {
                bool loaded = loadInstanceMesh(staticMeshData, *vdf, vob);
            }
            else {
                LOG(DEBUG) << "Skipping VOB " << visualname << " (visual not found)";
            }
        }

        if (debugStaticLights) {
            for (auto& light : lightsStatic) {
                float scale = light.range / 10.f;
                loadPointDebugVisual(staticMeshData, light.pos, { scale, scale, scale });
            }
        }
        if (debugStaticLightRays) {
            for (auto& ray : debugLightToVobRays) {
                loadLineDebugVisual(staticMeshData, ray.posStart, ray.posEnd, ray.color);
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
