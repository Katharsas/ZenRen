#pragma once

#include "../Renderer.h"

#include "vdfs/fileIndex.h"

namespace renderer::loader {

    struct ZenLightmapTexture {
        int32_t width;
        int32_t height;
        std::vector<uint8_t> ddsRaw;
    };

    std::vector<ZenLightmapTexture>& getLightmapTextures();

    RenderData loadZen(std::string& zenFilename, ZenLib::VDFS::FileIndex* vdf);
    //std::vector<ZenLightmapTexture> loadZenLightmaps();
    void clean();
}

