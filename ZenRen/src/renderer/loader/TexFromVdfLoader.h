#pragma once

#include "../Renderer.h"

#include "vdfs/fileIndex.h"
#include "zenload/zCMesh.h"

namespace renderer::loader
{
	InMemoryTexFile loadTex(const std::string& texFilename, const ZenLib::VDFS::FileIndex* vdf);
	std::vector<InMemoryTexFile> loadZenLightmaps(const ZenLib::ZenLoad::zCMesh* worldMesh);
}

