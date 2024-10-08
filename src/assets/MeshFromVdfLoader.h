#pragma once

#include "AssetCache.h"
#include "render/Loader.h"

#include "zenload/zCMesh.h"
#include "zenload/zCProgMeshProto.h"

namespace assets
{
    void loadWorldMesh(
        render::VERT_CHUNKS_BY_MAT& target,
        ZenLib::ZenLoad::zCMesh* worldMesh,
        bool debugChecksEnabled = false);
    
    void loadInstanceMesh(
        render::VERT_CHUNKS_BY_MAT& target,
        const ZenLib::ZenLoad::zCProgMeshProto& mesh,
        const ZenLib::ZenLoad::PackedMesh packedMesh,
        const render::StaticInstance& instance,
        bool debugChecksEnabled = false);

    void loadInstanceMeshLib(
        render::VERT_CHUNKS_BY_MAT& target,
        const MeshLibData& libData,
        const render::StaticInstance & instance,
        bool debugChecksEnabled = false);
}

