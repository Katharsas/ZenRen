#pragma once

#include "../Renderer.h"
#include "vdfs/fileIndex.h"

#include "DirectXTex.h"
#include "zenload/zTypes.h"
#include "zenload/zCMesh.h"
#include "zenload/zCProgMeshProto.h"

namespace renderer::loader {
    
    struct SoftwareLightmapTexture {
        int32_t width;
        int32_t height;
        //std::vector<uint8_t> ddsRaw;
        DirectX::Image* image;
    };

    void loadWorldMesh(
        std::unordered_map<Material, std::vector<WORLD_VERTEX>>& target,
        ZenLib::ZenLoad::zCMesh& worldMesh,
        std::vector<SoftwareLightmapTexture>& lightmapTextures);
    
    void loadInstanceMesh(
        std::unordered_map<Material, std::vector<POS_NORMAL_UV>>& target,
        ZenLib::ZenLoad::zCProgMeshProto& mesh);
}

