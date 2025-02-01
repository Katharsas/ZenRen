#pragma once

#include "AssetCache.h"
#include "render/Loader.h"

#include "zenkit/World.hh"
#include "zenkit/MultiResolutionMesh.hh"
#include "zenkit/ModelHierarchy.hh"
#include "zenkit/ModelMesh.hh"

namespace assets
{
    void loadWorldMesh(
        render::VERT_CHUNKS_BY_MAT& target,
        const zenkit::Mesh& worldMesh,
        bool debugChecksEnabled = false);
    
    void loadInstanceMesh(
        render::VERT_CHUNKS_BY_MAT& target,
        const zenkit::MultiResolutionMesh& mesh,
        const render::StaticInstance& instance,
        bool debugChecksEnabled);

    void loadInstanceModel(
        render::VERT_CHUNKS_BY_MAT& target,
        const zenkit::ModelHierarchy& hierarchy,
        const zenkit::ModelMesh& model,
        const render::StaticInstance& instance,
        bool debugChecksEnabled);

    void loadInstanceDecal(
        render::VERT_CHUNKS_BY_MAT& target,
        const render::StaticInstance& instance,
        bool debugChecksEnabled
    );

    void printAndResetLoadStats(bool debugChecksEnabled);
}
