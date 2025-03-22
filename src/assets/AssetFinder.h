#pragma once

#include "Util.h"
#include "render/Loader.h"

#include <filesystem>

#undef ERROR
#include <zenkit/Vfs.hh>
#include <zenkit/Stream.hh>

namespace assets
{
	struct AssetFormats {
		std::initializer_list<util::FileExt> formatsFile;
		std::initializer_list<util::FileExt> formatsVfs;
	};

	const AssetFormats FORMATS_TEXTURE = {
		{
			FormatsSource::TGA,
			FormatsSource::PNG,
			FormatsCompiled::TEX
		}, {
			FormatsCompiled::TEX
		}
	};
	const AssetFormats FORMATS_LEVEL = {
		{
			FormatsCompiled::ZEN
		}, {
			FormatsCompiled::ZEN
		}
	};

	enum class AssetsIntern {
		DEFAULT_TEXTURE
	};

	// abstracts over real vs VFS files, allows getting byte contents
	struct FileHandle {
		// TODO forward declare the pointers and move to Loader.h
		const std::string name = "";
		const std::filesystem::path* path = nullptr;// -> if not null, this is a real file
		const zenkit::VfsNode* node = nullptr;// -> if not null, this is a ZenKit VFS entry
	};

	const render::FileData getData(const FileHandle handle);

	FileHandle getInternal(const AssetsIntern asset);
	std::optional<FileHandle> getIfExistsAsFile(const std::string_view assetNameLower);
	std::optional<FileHandle> getIfExistsInVfs(const std::string_view assetNameLower);
	std::optional<FileHandle> getIfExists(const std::string_view assetNameLower);
	std::optional<std::pair<FileHandle, util::FileExt>> getIfAnyExists(const std::string_view assetNameAnyCase, const AssetFormats& formats);
	bool exists(const std::string_view assetName);

	void initAssetsIntern();
	void initFileAssetSourceDir(std::filesystem::path& rootDir);
	void initVdfAssetSourceDir(std::filesystem::path& rootDir);
	void cleanAssetSources();
	void printFoundZens();
}
