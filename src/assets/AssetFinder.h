#pragma once

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

	struct FileData {
		// TODO wrap zenkit::Mmap in unique_ptr so we can convert forward declare the pointers and move this to Loader.h
		
		// not copyable, must be moved
		FileData(const FileData&) = delete;

		// backed by some static buffer
		FileData(const std::byte* data, uint64_t size)
			: data(data), size(size) {}

		// backed by mmap which is kept open for this objects lifetime
		FileData(const std::byte* data, uint64_t size, zenkit::Mmap&& mmap)
			: data(data), size(size), mmap(std::move(mmap)) {}

		// backed by heap buffer which is kept for this objects lifetime
		FileData(const std::byte* data, uint64_t size, std::unique_ptr<std::vector<std::uint8_t>>&& buffer)
			: data(data), size(size), buffer(std::move(buffer)) {}

		const std::byte* data = nullptr;
		const uint64_t size = 0;

		// both zenkit::Mmap and std::unique_ptr are not copyable, making this struct also not copyable
		// both are automatically deleted when this struct goes out of scope or is deleted

		const std::optional<zenkit::Mmap> mmap = std::nullopt;
		const std::optional<std::unique_ptr<std::vector<std::uint8_t>>> buffer;
	};

	const FileData getData(const FileHandle handle);

	std::optional<FileHandle> getIfExistsAsFile(const std::string_view assetName);
	std::optional<FileHandle> getIfExistsInVfs(const std::string_view assetName);
	std::optional<FileHandle> getIfExists(const std::string_view assetName);
	bool exists(const std::string_view assetName);

	void initFileAssetSourceDir(std::filesystem::path& rootDir);
	void initVdfAssetSourceDir(std::filesystem::path& rootDir);

	[[deprecated("Use getIfExists to get source-independent FileHandle to any asset data (VFS or not)!")]]
	const Vfs& getVfsLegacy();
}
