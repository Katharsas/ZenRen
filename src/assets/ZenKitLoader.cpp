#include "stdafx.h"
#include "ZenKitLoader.h"

#include "render/MeshUtil.h"
#include "assets/VobLoader.h"
#include "assets/LookupTrees.h"
#include "../render/PerfStats.h"
#include "Util.h"
#include "AssetCache.h"
#include "MeshFromVdfLoader.h";
#include "TexFromVdfLoader.h"

#include "zenkit/World.hh"
#include "zenkit/vobs/Light.hh"
//#include "zenkit/Mesh.hh"

#include <glm/gtc/type_ptr.hpp>

namespace assets
{
    using namespace render;
    using namespace DirectX;
    using std::string;
    using std::vector;
    using std::shared_ptr;
    using ::util::FileExt;
    using ::util::endsWithEither;

    void forEachVob(const vector<shared_ptr<zenkit::VirtualObject>>& vobs, std::function<void(zenkit::VirtualObject const *const)> processVob)
    {
        for (auto vobPtr : vobs) {
            processVob(vobPtr.get());
            forEachVob(vobPtr->children, processVob);
        }
    }

    vector<Light> loadLights(const vector<shared_ptr<zenkit::VirtualObject>>& rootVobs)
    {
        vector<Light> lights;
        forEachVob(rootVobs, [&](zenkit::VirtualObject const* const vobPtr) -> void {
            const zenkit::VirtualObject& vob = *vobPtr;
            bool isLight = vob.type == zenkit::VirtualObjectType::zCVobLight;
            if (isLight) {
                const zenkit::VLight& vobLight = *dynamic_cast<zenkit::VLight const * const>(vobPtr);
                bool isStatic = vobLight.is_static;
                if (isStatic) {
                    XMVECTOR pos = toXM4Pos(vob.position);
                    Light light = {
                        .pos = toVec3(pos * G_ASSET_RESCALE),
                        .isStatic = true,
                        .color = fromSRGB(from4xUint8(glm::value_ptr(vobLight.color))),
                        .range = vobLight.range * G_ASSET_RESCALE
                    };
                    lights.push_back(light);
                }
            }
        });

        LOG(INFO) << "VOBs: Loaded " << lights.size() << " static lights.";
        return lights;
    }

    bool loadVob(zenkit::VirtualObject const* const vobPtr)
    {
        using namespace FormatsSource;

        const zenkit::VirtualObject& vob = *vobPtr;
        const auto& visualName = vob.visual->name;
        if (vob.show_visual && !visualName.empty()) {
            if (endsWithEither(visualName, { __3DS, ASC, MDS })) {
                return true;
            }
            if (endsWithEither(visualName, { PFX, MMS, TGA })) {
                // TODO effects, morphmeshes, decals
            }
            else {
                LOG(INFO) << "Visual not supported: " << vob.visual;
            }
        }
        return false;
    }

    vector<StaticInstance> loadVobs(
        vector<std::shared_ptr<zenkit::VirtualObject>>& rootVobs,
        const VERT_CHUNKS_BY_MAT& worldMeshData,
        const vector<Light>& lightsStatic,
        const bool isOutdoorLevel,
        LoadDebugFlags debug)
    {
        vector<StaticInstance> statics;

        const FaceLookupContext worldMeshContext = { createVertLookup(worldMeshData), worldMeshData };
        const LightLookupContext lightsStaticContext = { createLightLookup(lightsStatic), lightsStatic };

        forEachVob(rootVobs, [&](zenkit::VirtualObject const* const vobPtr) -> void {
            if (!loadVob(vobPtr)) {
                return;
            }
            const zenkit::VirtualObject& vob = *vobPtr;
            StaticInstance instance;
            instance.meshName = vob.visual->name;
            {
                glm::mat4x4 rotation(vob.rotation);
                // TODO we have no idea if this is row or column first, so may need to transpose.
                XMMATRIX rotate = XMMATRIX(glm::value_ptr(rotation));
                
                XMVECTOR pos = toXM4Pos(toVec3(vob.position, G_ASSET_RESCALE));
                XMMATRIX translate = XMMatrixTranslationFromVector(pos);
                instance.transform = XMMatrixMultiply(rotate, translate);
            }
            instance.bbox = {
                    toXM4Pos(toVec3(vob.bbox.min, G_ASSET_RESCALE)),
                    toXM4Pos(toVec3(vob.bbox.max, G_ASSET_RESCALE))
            };
            {
            VobLighting lighting = calculateStaticVobLighting(instance.bbox, worldMeshContext, lightsStaticContext, isOutdoorLevel, debug);
            if (debug.vobsTint) {
                lighting.color.r = (lighting.color.r / 3.f) * 2.f;
            }
            instance.lighting = lighting;
           }
           statics.push_back(instance);
        });

        return statics;
    }

    bool loadInstanceVisual(VERT_CHUNKS_BY_MAT& target, const StaticInstance& instance)
    {
        // TODO right now, getVobVisual essentially retrieves asset handle every time it is called,
        // doing the work of resolving the entire asset even if it is already in AssetCache which
        // does not make any sense.
        // Maybe getVobVisual should return a variant of an already parsed visual.
        
        // TODO ok none of this makes sense anyway because MDS source files map to either MDL or (MDH + MDM) !!
        // so just returning one compile visual for each source visual is already wrong.

        const auto assetNameOpt = getVobVisual(instance.meshName);
        if (!assetNameOpt.has_value()) {
            LOG(DEBUG) << "Visual type not supported! Skipping VOB " << instance.meshName;
            return false;
        }

        auto& assetName = assetNameOpt.value();

        if (endsWithEither(assetName, { FormatsCompiled::MRM })) {
            auto meshOpt = getOrParseMrm(assetName);
            assert(meshOpt.has_value());
            loadInstanceMeshZkit(target, *meshOpt.value(), instance, true);
        }
        else if (endsWithEither(assetName, { FormatsCompiled::MDL })) {
            auto meshOpt = getOrParseMdl(assetName);
            assert(meshOpt.has_value());
            loadInstanceModelZkit(target, meshOpt.value()->hierarchy, meshOpt.value()->mesh, instance, true);
        }
        else if (endsWithEither(assetName, { FormatsCompiled::MDM })) {
            //LOG(INFO) << "Loading: " << assetName;

            auto meshOpt = getOrParseMdm(assetName);
            assert(meshOpt.has_value());

            auto mdhAssetName = util::replaceExtension(instance.meshName, ".MDH");
            auto meshMdhOpt = getOrParseMdh(mdhAssetName);
            assert(meshMdhOpt.has_value());

            loadInstanceModelZkit(target, *meshMdhOpt.value(), *meshOpt.value(), instance, true);
        }
        else {
            LOG(DEBUG) << "Visual type not supported! Skipping VOB " << instance.meshName << " (" << assetName << ")";
            return false;
        }
        return true;
    }

    void loadZen2(render::RenderData& out, const FileHandle& levelFile, LoadDebugFlags debug)
    {
        LOG(INFO) << "Loading World!";
        auto sampler = render::stats::TimeSampler();
        sampler.start();

        auto fileData = assets::getData(levelFile);
        zenkit::World world{};
        {
            auto read = zenkit::Read::from(fileData.data, fileData.size);
            world.load(read.get());
        }
        bool isOutdoorLevel = world.world_bsp_tree.mode == zenkit::BspTreeType::OUTDOOR;
        sampler.logMillisAndRestart("Loader: World data parsed");

        loadWorldMeshZkit(out.worldMesh, world.world_mesh, true);
        sampler.logMillisAndRestart("Loader: World mesh loaded");

        vector<InMemoryTexFile> lightmaps = loadLightmapsZkit(world.world_mesh);
        sampler.logMillisAndRestart("Loader: World lightmaps loaded");

        vector<Light> lightsStatic = loadLights(world.world_vobs);

        // TODO debug lightmap VOB lighting
        vector<StaticInstance> vobs;
        vobs = loadVobs(world.world_vobs, out.worldMesh, lightsStatic, isOutdoorLevel, debug);
        sampler.logMillisAndRestart("Loader: World VOB data loaded");

        for (auto& instance : vobs) {
            loadInstanceVisual(out.staticMeshes, instance);
        }

        sampler.logMillisAndRestart("Loader: VOB visuals loaded");

        out.isOutdoorLevel = isOutdoorLevel;
        out.worldMeshLightmaps = lightmaps;
    }
}