#pragma once

#include "render/Loader.h"

#include "AssetFinder.h"
//#include "vdfs/fileIndex.h"

namespace assets
{
    void loadZen(render::RenderData& out, std::string& zenFilename, const Vfs vfs);
}

