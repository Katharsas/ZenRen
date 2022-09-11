#pragma once

#include "../Renderer.h"

namespace renderer::loader {

    struct ZenLightmapTexture {
        int32_t width;
        int32_t height;
        std::vector<uint8_t> ddsRaw;
    };

    std::unordered_map<std::string, std::vector<POS_NORMAL_UV>> loadZen();
    std::vector<ZenLightmapTexture> loadZenLightmaps();
    void clean();
}

