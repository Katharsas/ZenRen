#include "stdafx.h"
#include "ZenLoader.h"

#include "vdfs/fileIndex.h"
#include "zenload/zCMesh.h"
#include "zenload/zenParser.h"

namespace renderer::loader {

    using namespace ZenLib;

	void loadZen() {
        const std::string vdfsArchiveToLoad = "data_g1/worlds.VDF";
        const std::string zenFilename = "WORLD.ZEN";

        VDFS::FileIndex::initVDFS("Foo");
        VDFS::FileIndex vdf;
        vdf.loadVDF(vdfsArchiveToLoad);
        vdf.finalizeLoad();

        // Initialize parser with zenfile from vdf
        ZenLoad::ZenParser parser(zenFilename, vdf);

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

        ZenLoad::PackedMesh packedWorldMesh;
        parser.getWorldMesh()->packMesh(packedWorldMesh, 0.01f);

        LOG(INFO) << "Zen loaded!";

        // Print some sample-data for vobs which got a visual
        /*
        std::cout << "Listing vobs..." << std::endl;
        listVobInformation(world.rootVobs);

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
