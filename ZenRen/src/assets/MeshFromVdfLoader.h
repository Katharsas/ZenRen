#pragma once

#include "render/Renderer.h"
#include "vdfs/fileIndex.h"

#include "DirectXTex.h"
#include "zenload/zTypes.h"
#include "zenload/zCMesh.h"
#include "zenload/zCProgMeshProto.h"

namespace assets
{
    void loadWorldMesh(
        render::VERTEX_DATA_BY_MAT& target,
        ZenLib::ZenLoad::zCMesh* worldMesh);
    
    void loadInstanceMesh(
        render::VERTEX_DATA_BY_MAT& target,
        const ZenLib::ZenLoad::zCProgMeshProto& mesh,
        const render::StaticInstance& instance,
        bool debugChecksEnabled = false);
}

