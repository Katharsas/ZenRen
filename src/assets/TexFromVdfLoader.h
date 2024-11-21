#pragma once

#include "render/Loader.h"

#include "AssetFinder.h"
#include "vdfs/fileIndex.h"
#include "zenload/zCMesh.h"

namespace assets
{
	render::InMemoryTexFile loadTex(const std::string& texFilename, const Vfs vfs);
	std::vector<render::InMemoryTexFile> loadZenLightmaps(const ZenLib::ZenLoad::zCMesh* worldMesh);
}

