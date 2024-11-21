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
		//union {
		ZenLib::VDFS::FileIndex* zlib = nullptr;
		zenkit::Vfs* zkit = nullptr;
		//};
		bool initialized() const
		{
			return zlib != nullptr || zkit != nullptr;
		}
		bool hasFile(const std::string& name) const
		{
			if (zlib != nullptr) {
				return zlib->hasFile(name);
			}
			else {
				//return zkit->find(name) != nullptr;
				return false;
			}
		}
		void getFileData(const std::string& file, std::vector<uint8_t>& data) const
		{
			if (zlib != nullptr) {
				zlib->getFileData(file, data);
			}
			else {
				//const auto node = zkit->find(file);
				//std::unique_ptr<zenkit::Read> read = node->open_read();
				//read->read(data.data(), data.size());
			}
		}
	};

	std::optional<Vfs> getVfsIndex();
	std::optional<const std::filesystem::path*> existsAsFile(const std::string_view assetName);
	void initFileAssetSourceDir(std::filesystem::path& rootDir);
	void initVdfAssetSourceDir(std::filesystem::path& rootDir);
}
