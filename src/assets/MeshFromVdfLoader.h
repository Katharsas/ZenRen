#pragma once

#include "AssetCache.h"
#include "render/Loader.h"

#include "zenload/zCMesh.h"
#include "zenload/zCProgMeshProto.h"

#include "zenkit/World.hh"

namespace assets
{
    void loadWorldMeshZkit(
        render::VERT_CHUNKS_BY_MAT& target,
        const zenkit::Mesh& worldMesh,
        bool debugChecksEnabled = false);

    void loadWorldMesh(
        render::VERT_CHUNKS_BY_MAT& target,
        ZenLib::ZenLoad::zCMesh* worldMesh,
        bool debugChecksEnabled = false);
    
    void loadInstanceMesh(
        render::VERT_CHUNKS_BY_MAT& target,
        const ZenLib::ZenLoad::zCProgMeshProto& mesh,
        const render::StaticInstance& instance,
        bool debugChecksEnabled = false);

    void loadInstanceMeshLib(
        render::VERT_CHUNKS_BY_MAT& target,
        const MeshLibData& libData,
        const render::StaticInstance & instance,
        bool debugChecksEnabled = false);
}

