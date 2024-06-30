#pragma once

#include "render/Renderer.h"

#include "vdfs/fileIndex.h"
#include "zenload/zCMesh.h"

namespace assets
{
	render::InMemoryTexFile loadTex(const std::string& texFilename, const ZenLib::VDFS::FileIndex* vdf);
	std::vector<render::InMemoryTexFile> loadZenLightmaps(const ZenLib::ZenLoad::zCMesh* worldMesh);
}

