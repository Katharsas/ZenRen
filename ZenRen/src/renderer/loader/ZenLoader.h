#pragma once

#include "../Renderer.h"

#include "vdfs/fileIndex.h"

namespace renderer::loader {
    RenderData loadZen(std::string& zenFilename, ZenLib::VDFS::FileIndex* vdf);
}

