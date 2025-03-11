#pragma once

#include "AssetFinder.h"
#include "render/Loader.h"
#include <zenkit/Vfs.hh>

namespace assets
{
    void loadZen(render::RenderData& out, const FileHandle& levelFile, LoadDebugFlags debug);
}