#pragma once

#include "render/Loader.h"

#include <filesystem>

#undef ERROR
#include <zenkit/Vfs.hh>
#include <zenkit/Stream.hh>

namespace assets
{
	const std::filesystem::path DEFAULT_TEXTURE = "./default_texture.png";

	// abstracts over real vs VFS files, allows getting byte contents
	struct FileHandle {
		// TODO forward declare the pointers and move to Loader.h
		const std::string name = "";
		const std::filesystem::path* path = nullptr;// -> if not null, this is a real file
		const zenkit::VfsNode* node = nullptr;// -> if not null, this is a ZenKit VFS entry
	};

	const render::FileData getData(const FileHandle handle);

	std::optional<FileHandle> getIfExistsAsFile(const std::string_view assetName);
	std::optional<FileHandle> getIfExistsInVfs(const std::string_view assetName);
	std::optional<FileHandle> getIfExists(const std::string_view assetName);
	bool exists(const std::string_view assetName);

	void initFileAssetSourceDir(std::filesystem::path& rootDir);
	void initVdfAssetSourceDir(std::filesystem::path& rootDir);
}
