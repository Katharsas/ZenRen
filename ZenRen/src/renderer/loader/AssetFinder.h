#pragma once

#include <filesystem>

#include "vdfs/fileIndex.h"

namespace renderer::loader
{
	const std::filesystem::path DEFAULT_TEXTURE = "./default_texture.png";

	std::optional<ZenLib::VDFS::FileIndex*> getVdfIndex();
	std::optional<const std::filesystem::path*> existsAsFile(const std::string& assetName);
	void initFileAssetSourceDir(std::filesystem::path& rootDir);
	void initVdfAssetSourceDir(std::filesystem::path& rootDir);
}
