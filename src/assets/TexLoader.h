#pragma once

//#include "AssetFinder.h"
#include "render/Dx.h"
#include "render/Loader.h"
#include "render/Texture.h"

//#undef ERROR
//#include "zenkit/Stream.hh"
//#include "zenkit/Mesh.hh"

namespace assets
{
	render::Texture* createTextureFromImageFormat(
		render::D3d d3d, const render::FileData& imageFile, bool srgb);

	render::Texture* createTextureFromGothicTex(
		render::D3d d3d, const render::FileData& cTexFile, bool srgb);

	render::Texture* createTextureOrDefault(
		render::D3d d3d, const std::string& assetName, bool srgb);

	std::vector<render::Texture*> createTexturesFromLightmaps(
		render::D3d d3d, const std::vector<render::FileData>& lightmapFiles);

	// TODO consider re-compressing rebuilt, decompressed or uncompressed textures
	// -> mixing TEX and image file textures will reduce batching efficiency due to incompatible formats
}

