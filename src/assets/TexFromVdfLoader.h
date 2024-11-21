#pragma once

#include "render/Loader.h"

#include "AssetFinder.h"

#undef ERROR
#include "zenkit/Stream.hh"
#include "zenkit/Mesh.hh"

#include "vdfs/fileIndex.h"
#include "zenload/zCMesh.h"

namespace assets
{
	std::optional<render::InMemoryTexFile> loadTex(const FileHandle& file);
	std::vector<render::InMemoryTexFile> loadZenLightmaps(const ZenLib::ZenLoad::zCMesh* worldMesh);
	std::vector<render::InMemoryTexFile> loadLightmapsZkit(const zenkit::Mesh& worldMesh);
}

