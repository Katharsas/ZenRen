#include "stdafx.h"
#include "TexFromVdfLoader.h"

#include "zenload/ztex2dds.h"

namespace assets
{
    using namespace ZenLib;
    using ::render::InMemoryTexFile;
    using ::std::string;
    using ::std::vector;

    InMemoryTexFile loadTex(const string& texFilename, const VDFS::FileIndex* vdf) {
        vector<uint8_t> texRaw;
        vdf->getFileData(texFilename, texRaw);

        InMemoryTexFile tex;
        int32_t message = ZenLoad::convertZTEX2DDS(texRaw, tex.ddsRaw, false, &tex.width, &tex.height);
        if (message != 0) {
            LOG(WARNING) << "Failed to convert zTex '" << texFilename << "' to DDS: Error code '" << message << "'!";
        }
        return tex;
    }

    vector<InMemoryTexFile> loadZenLightmaps(const ZenLoad::zCMesh* worldMesh) {
        vector<InMemoryTexFile> result;

        for (auto& lightmap : worldMesh->getLightmapTextures()) {
            InMemoryTexFile tex;
            int32_t message = ZenLoad::convertZTEX2DDS(lightmap, tex.ddsRaw, false, &tex.width, &tex.height);
            if (message != 0) {
                LOG(WARNING) << "Failed to convert lightmap zTex to DDS: Error code '" << message << "'!";
            }
            result.push_back(tex);
        }
        return result;
    }
}
