#pragma once

#include "../Renderer.h"

namespace renderer::loader {

    struct ZenLightmapTexture {
        int32_t width;
        int32_t height;
        std::vector<uint8_t> ddsRaw;
    };

    std::vector<ZenLightmapTexture>& getLightmapTextures();

    std::unordered_map<Material, std::vector<WORLD_VERTEX>> loadZen();
    //std::vector<ZenLightmapTexture> loadZenLightmaps();
    void clean();
}

