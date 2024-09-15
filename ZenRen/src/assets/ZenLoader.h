#pragma once

#include "render/Renderer.h"

#include "vdfs/fileIndex.h"

namespace assets
{
    void loadZen(render::RenderData& out, std::string& zenFilename, ZenLib::VDFS::FileIndex* vdf);
}

