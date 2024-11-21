#pragma once

#include "AssetFinder.h"
#include "render/Loader.h"
#include <zenkit/Vfs.hh>

namespace assets
{
    // TODO of course, passing FileHandle here only really makes sense if this function is abstract over both ZenKit and ZenLib
    void loadZen2(render::RenderData& out, const FileHandle& levelFile, LoadDebugFlags debug = {});
}