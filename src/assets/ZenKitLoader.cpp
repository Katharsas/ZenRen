#include "stdafx.h"
#include "ZenKitLoader.h"

#include "../render/PerfStats.h"
#include "Util.h"
#include "AssetCache.h"
#include "MeshFromVdfLoader.h";
#include "TexFromVdfLoader.h"

#include "zenkit/World.hh"
//#include "zenkit/Mesh.hh"

namespace assets
{
    using namespace render;
    using std::string;
    using std::vector;
    using ::util::FileExt;

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

    void loadZen2(render::RenderData& out, const FileHandle& levelFile)
    {
        // TODO maybe we should already use zenkit::Read as argument??
        LOG(INFO) << "Loading World!";

        auto sampler = render::stats::TimeSampler();
        sampler.start();

        //zenkit::VfsNode const* zenNode = vfs.zkit->find(zenFilename);
        //if (zenNode == nullptr) {
        //    LOG(FATAL) << "ZEN-File not found!";
        //}
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

        // TODO merge this file into ZenLoader

        out.isOutdoorLevel = isOutdoorLevel;
        out.worldMeshLightmaps = lightmaps;
    }
}