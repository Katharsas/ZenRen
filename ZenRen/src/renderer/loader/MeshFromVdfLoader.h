#pragma once

#include "../Renderer.h"
#include "vdfs/fileIndex.h"

#include "DirectXTex.h"
#include "zenload/zTypes.h"
#include "zenload/zCMesh.h"
#include "zenload/zCProgMeshProto.h"

namespace renderer::loader
{
    void loadWorldMesh(
        std::unordered_map<Material, VEC_POS_NORMAL_UV_LMUV>& target,
        ZenLib::ZenLoad::zCMesh* worldMesh);
    
    void loadInstanceMesh(
        std::unordered_map<Material, VEC_POS_NORMAL_UV_LMUV>& target,
        const ZenLib::ZenLoad::zCProgMeshProto& mesh,
        const DirectX::XMMATRIX& transform,
        const std::string& visualname,
        bool debugChecksEnabled = false);
}

