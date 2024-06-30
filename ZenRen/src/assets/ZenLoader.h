#pragma once

#include "render/Renderer.h"

#include "vdfs/fileIndex.h"

namespace assets
{
    render::RenderData loadZen(std::string& zenFilename, ZenLib::VDFS::FileIndex* vdf);
}

