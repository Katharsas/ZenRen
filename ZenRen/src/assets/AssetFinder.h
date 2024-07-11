#pragma once

#include <filesystem>

#include "vdfs/fileIndex.h"

namespace assets
{
	const std::filesystem::path DEFAULT_TEXTURE = "./default_texture.png";

	std::optional<ZenLib::VDFS::FileIndex*> getVdfIndex();
	std::optional<const std::filesystem::path*> existsAsFile(const std::string_view assetName);
	void initFileAssetSourceDir(std::filesystem::path& rootDir);
	void initVdfAssetSourceDir(std::filesystem::path& rootDir);
}
