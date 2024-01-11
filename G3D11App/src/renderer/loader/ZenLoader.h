#pragma once

#include "../Renderer.h"

namespace renderer::loader {

    struct ZenLightmapTexture {
        int32_t width;
        int32_t height;
        std::vector<uint8_t> ddsRaw;
    };

    std::vector<ZenLightmapTexture>& getLightmapTextures();

    RenderData loadVdfs();
    //std::vector<ZenLightmapTexture> loadZenLightmaps();
    void clean();
}

