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
        render::MatToChunksToVertsBasic& target,
        const zenkit::Mesh& worldMesh,
        bool indexed,
        bool debugChecksEnabled = false);
    
    void loadInstanceMesh(
        render::MatToChunksToVertsBasic& target,
        const zenkit::MultiResolutionMesh& mesh,
        const render::StaticInstance& instance,
        bool indexed,
        bool debugChecksEnabled);

    void loadInstanceModel(
        render::MatToChunksToVertsBasic& target,
        const zenkit::ModelHierarchy& hierarchy,
        const zenkit::ModelMesh& model,
        const render::StaticInstance& instance,
        bool indexed,
        bool debugChecksEnabled);

    void loadInstanceDecal(
        render::MatToChunksToVertsBasic& target,
        const render::StaticInstance& instance,
        bool indexed,
        bool debugChecksEnabled
    );

    void printAndResetLoadStats(bool debugChecksEnabled);
}
