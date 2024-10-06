#pragma once

#include "render/Loader.h"

#include "vdfs/fileIndex.h"

namespace assets
{
    void loadZen(render::RenderData& out, std::string& zenFilename, ZenLib::VDFS::FileIndex* vdf);
}

