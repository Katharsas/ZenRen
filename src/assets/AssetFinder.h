#pragma once

#include "render/Loader.h"

#include <filesystem>

#include "vdfs/fileIndex.h"
#undef ERROR
#include <zenkit/Vfs.hh>
#include <zenkit/Stream.hh>

namespace assets
{
	const std::filesystem::path DEFAULT_TEXTURE = "./default_texture.png";

	struct Vfs {
		ZenLib::VDFS::FileIndex* zlib = nullptr;
		zenkit::Vfs* zkit = nullptr;
		
		bool initialized() const
		{
			return zlib != nullptr || zkit != nullptr;
		}
	};

	// abstracts over real vs VFS files, allows getting byte contents
	struct FileHandle {
		// TODO forward declare the pointers and move to Loader.h
		const std::string name = "";
		const std::filesystem::path* path = nullptr;// -> if not null, this is a real file
		const zenkit::VfsNode* node = nullptr;// -> if not null, this is a ZenKit VFS entry
		//const std::string nodeZlib;// -> if not empty, this is a ZenLib VFS entry
	};

	const render::FileData getData(const FileHandle handle);

	std::optional<FileHandle> getIfExistsAsFile(const std::string_view assetName);
	std::optional<FileHandle> getIfExistsInVfs(const std::string_view assetName);
	std::optional<FileHandle> getIfExists(const std::string_view assetName);
	bool exists(const std::string_view assetName);

	void initFileAssetSourceDir(std::filesystem::path& rootDir);
	void initVdfAssetSourceDir(std::filesystem::path& rootDir);

	[[deprecated("Use getIfExists to get source-independent FileHandle to any asset data (VFS or not)!")]]
	const Vfs& getVfsLegacy();
}
