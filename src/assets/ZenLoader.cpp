#include "stdafx.h"
#include "ZenLoader.h"

#include "Util.h"
#include "render/PerfStats.h"
#include "render/MeshUtil.h"

#include "assets/VobLoader.h"
#include "assets/LookupTrees.h"
#include "assets/AssetCache.h"
#include "assets/MeshFromVdfLoader.h";
#include "assets/TexLoader.h"

#include "zenkit/World.hh"
#include "zenkit/vobs/Light.hh"

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
        // TODO we should probably use visual type here, not extensions
        using namespace FormatsSource;

        const zenkit::VirtualObject& vob = *vobPtr;
        const auto& visualName = vob.visual->name;
        if (vob.show_visual && !visualName.empty()) {
            if (endsWithEither(visualName, { __3DS, ASC, MDS, TGA })) {
                return true;
            }
            if (endsWithEither(visualName, { PFX, MMS })) {
                // TODO effects, morphmeshes, decals
            }
            else {
                LOG(INFO) << "Visual not supported: " << visualName;
            }
        }
        return false;
    }

    vector<StaticInstance> loadVobs(
        vector<std::shared_ptr<zenkit::VirtualObject>>& rootVobs,
        const MatToChunksToVertsBasic& worldMeshData,
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
            instance.type = (VisualType) vob.visual->type;
            instance.visual_name = vob.visual->name;
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
            if (vob.visual->type == zenkit::VisualType::DECAL) {
                //assert(::util::endsWith(instance.meshName, ".TGA"));
                const zenkit::VisualDecal* decalVisual = dynamic_cast<zenkit::VisualDecal*>(vobPtr->visual.get());
                instance.decal = Decal{
                    .quad_size = toVec2(decalVisual->dimension),
                    .uv_offset = toUv(decalVisual->offset),
                    .two_sided = decalVisual->two_sided,
                    .alpha = decalVisual->alpha_func,
                };
            }
            {
                VobLighting lighting = calculateStaticVobLighting(instance.bbox, worldMeshContext, lightsStaticContext, isOutdoorLevel, debug);
                if (debug.vobsTint) {
                    lighting.color.r = (lighting.color.r / 3.f) * 2.f;
                }
                if (instance.decal.has_value()) {
                    // TODO figure out decal lighting, especially ADD, instead of hacking it in shader
                }
                instance.lighting = lighting;
            }
            statics.push_back(instance);
        });

        return statics;
    }

    bool loadInstanceVisual(MatToChunksToVertsBasic& target, const StaticInstance& instance, bool debugChecksEnabled)
    {
        using namespace FormatsSource;
        using namespace FormatsCompiled;
        const auto& name = instance.visual_name;

        switch (instance.type) {
        case VisualType::MULTI_RESOLUTION_MESH: {
            assert(__3DS.isExtOf(name));
            auto compiledName = ::util::replaceExtension(name, MRM.str());
            auto meshOpt = getOrParse<zenkit::MultiResolutionMesh>(compiledName);
            if (!meshOpt.has_value()) {
                LOG(INFO) << "Failed to find MRM data for visual: " << name;
                return false;
            }
            loadInstanceMesh(target, *meshOpt.value(), instance, debugChecksEnabled);
            return true;
        }
        case VisualType::MODEL: {
            bool isAsc = ASC.isExtOf(name);// ASCs are always compiled to MDL apparently (?)
            bool isMds = !isAsc && MDS.isExtOf(name);// MDS might be either MDL or MDH+MDM pair
            assert(isAsc || isMds);
            {
                // MDL
                auto compiledName = ::util::replaceExtension(name, MDL.str());
                auto meshOpt = getOrParse<zenkit::Model>(compiledName);
                if (!meshOpt.has_value()) {
                    if (isAsc) {
                        LOG(INFO) << "Failed to find MDL data for visual: " << name;
                        return false;
                    }
                    // try MDH+MDM
                }
                else {
                    loadInstanceModel(target, meshOpt.value()->hierarchy, meshOpt.value()->mesh, instance, debugChecksEnabled);
                    return true;
                }
            } {
                // MDH+MDM
                auto compiledName = ::util::replaceExtension(name, MDH.str());
                auto mdhOpt = getOrParse<zenkit::ModelHierarchy>(compiledName);
                compiledName = ::util::replaceExtension(name, MDM.str());
                auto mdmOpt = getOrParse<zenkit::ModelMesh>(compiledName);
                if (!mdhOpt.has_value() || !mdmOpt.has_value()) {
                    LOG(INFO) << "Failed to find MDH + MDM data for visual: " << name;
                    return false;
                }
                loadInstanceModel(target, *mdhOpt.value(), *mdmOpt.value(), instance, debugChecksEnabled);
                return true;
            }
        }
        case VisualType::DECAL: {
            assert(TGA.isExtOf(name));
            loadInstanceDecal(target, instance, debugChecksEnabled);
            return true;
        }
        default: {
            LOG(INFO) << "Visual not supported: " << name;
            return false;
        }
        }
    }

    void loadZen(render::RenderData& out, const FileHandle& levelFile, LoadDebugFlags debug)
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

        loadWorldMesh(out.worldMesh, world.world_mesh, true);

        for (uint32_t i = 0; i < world.world_mesh.lightmap_textures.size(); i++) {
            auto& lightmap = world.world_mesh.lightmap_textures.at(i);
            auto name = std::format("lightmap_{:03}.tex", i);
            out.worldMeshLightmaps.emplace_back(name, (std::byte*)lightmap->data(), lightmap->size(), lightmap);
        }
        sampler.logMillisAndRestart("Loader: World mesh loaded");

        vector<Light> lightsStatic = loadLights(world.world_vobs);

        // TODO debug lightmap VOB lighting
        vector<StaticInstance> vobs;
        vobs = loadVobs(world.world_vobs, out.worldMesh, lightsStatic, isOutdoorLevel, debug);
        sampler.logMillisAndRestart("Loader: World VOB data loaded");

        for (auto& instance : vobs) {
            loadInstanceVisual(out.staticMeshes, instance, debug.validateMeshData);
        }

        sampler.logMillisAndRestart("Loader: VOB visuals loaded");

        printAndResetLoadStats(debug.validateMeshData);
        out.isOutdoorLevel = isOutdoorLevel;
    }
}
