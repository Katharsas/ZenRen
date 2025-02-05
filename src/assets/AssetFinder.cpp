#include "stdafx.h"
#include "AssetFinder.h"

#include "DebugTextures.h"
#include "../Util.h"
#include <zenkit/Logger.hh>

#include <iostream>
#include <fstream>
#include <span>
#include <spanstream>

namespace assets
{
	namespace fs = std::filesystem;
	using render::FileData;
	using std::byte;
	using std::vector;
	using std::string;
	using std::string_view;

	std::unordered_map<string, const fs::path> assetNamesToPaths;
	std::unordered_set<string> zensFoundInVdfs;

	struct Vfs {
		zenkit::Vfs* zkit = nullptr;

		bool initialized() const
		{
			return zkit != nullptr;
		}
	} vfs;

	string filenameWithTga(const string& filename) {
		return util::replaceExtension(filename, ".tga");
	}

	void walkFilesRecursively(fs::path& rootDir, const std::function<void(const fs::path&, const string&)> onFile) {
		for (const auto& dirEntry : fs::recursive_directory_iterator(rootDir)) {
			if (!std::filesystem::is_directory(dirEntry)) {
				const auto& path = dirEntry.path();
				auto filename = util::toString(path.filename());
				util::asciiToLowerMut(filename);

				onFile(path, filename);
			}
		}
	}

	// for that we need AssetFinder to provide generic file byte streaming / in-memory-buffering and other code use that as input
	std::optional<FileHandle> getIfExistsAsFile(const string_view assetName)
	{
		auto assetNameStr = string(assetName);
		auto it = assetNamesToPaths.find(assetNameStr);
		if (it != assetNamesToPaths.end()) {
			return FileHandle{
				.name = std::move(assetNameStr),
				.path = &(it->second),
				.node = nullptr,
			};
		}
		else {
			return std::nullopt;
		}
	}

	std::optional<FileHandle> getIfExistsInVfs(const string_view assetName)
	{
		assert(vfs.initialized());
		std::string assetNameStr = string(assetName);
		const zenkit::VfsNode* node = vfs.zkit->find(assetNameStr);
		if (node != nullptr) {
			return FileHandle{
				.name = std::move(assetNameStr),
				.path = nullptr,
				.node = node,
			};
		}
		else {
			return std::nullopt;
		}
	}

	std::optional<FileHandle> getIfExists(const string_view assetName)
	{
		auto realFile = getIfExistsAsFile(assetName);
		if (realFile.has_value()) {
			return realFile;
		}
		else {
			return getIfExistsInVfs(assetName);
		}
	}

	bool exists(const string_view assetName)
	{
		return getIfExists(assetName).has_value();
	}

	std::unique_ptr<zenkit::Read> getDataStream(FileHandle handle)
	{
		if (handle.path != nullptr) {
			return zenkit::Read::from(*handle.path);
		}
		if (handle.node != nullptr) {
			return handle.node->open_read();
		}
	}

	const FileData getData(const FileHandle handle)
	{
		if (handle.path != nullptr) {
			auto mmap = zenkit::Mmap(*handle.path);
			return FileData(handle.name, mmap.data(), mmap.size(), std::move(mmap));
		}
		if (handle.node != nullptr) {
			phoenix::buffer buffer_view = handle.node->open();
			return FileData(handle.name, buffer_view.array(), buffer_view.limit());
		}
	}

	void initFileAssetSourceDir(fs::path& rootDir)
	{
		assetNamesToPaths.clear();
		LOG(INFO) << "Scanning dir for asset files: " << util::toString(rootDir);

		walkFilesRecursively(rootDir, [](const fs::path& path, const string& filename) -> void {
			// textures
			if (util::endsWith(filename, ".dds")) {
				assetNamesToPaths.insert(std::pair(filenameWithTga(filename), path));
			}
			if (util::endsWith(filename, ".png")) {
				assetNamesToPaths.insert(std::pair(filenameWithTga(filename), path));
			}
			if (util::endsWith(filename, ".tga")) {
				assetNamesToPaths.insert(std::pair(filename, path));
			}

			// meshes
			if (util::endsWith(filename, ".3ds")) {
				assetNamesToPaths.insert(std::pair(filename, path));
			}
		});

		assets::initDebugTextures(&assetNamesToPaths);
	}

	void initVdfAssetSourceDir(fs::path& rootDir)
	{
		LOG(INFO) << "Scanning dir for VDF files: " << util::toString(rootDir);
		
		zenkit::Logger::use_logger([&](zenkit::LogLevel logLevel, std::string const& message) -> void {
			switch (logLevel) {
			case zenkit::LogLevel::TRACE: LOG(DEBUG) << "zenkit [TRACE]: " << message; break;
			case zenkit::LogLevel::DEBUG: LOG(DEBUG) << "zenkit: " << message; break;
			case zenkit::LogLevel::INFO: LOG(INFO) << "zenkit: " << message; break;
			case zenkit::LogLevel::WARNING: LOG(WARNING) << "zenkit: " << message; break;
			case zenkit::LogLevel::ERROR: LOG(FATAL) << "zenkit: " << message; break;
			}
		});

		if (vfs.zkit != nullptr) {
			delete vfs.zkit;
		}
		vfs.zkit = new zenkit::Vfs();
			
		walkFilesRecursively(rootDir, [](const fs::path& path, const std::string& filename) -> void {
			try {
				if (util::endsWith(filename, ".vdf")) {
					vfs.zkit->mount_disk(path);
					LOG(DEBUG) << "Loaded VDF: " << filename;
				}
				if (util::endsWith(filename, ".mod")) {
					vfs.zkit->mount_disk(path);
					LOG(DEBUG) << "Loaded MOD: " << filename;
				}
			}
			catch (...) {
				LOG(WARNING) << "Skipped unsupported VDF: " << filename;
			}
		});

		const auto& vdfFileList = vfs.zkit->root().children();
		for (const auto& node : vdfFileList) {
			LOG(INFO) << "VFS:  " << node.name();
			const auto filenameLow = util::asciiToLower(node.name());
			if (util::endsWith(filenameLow, ".zen")) {
				zensFoundInVdfs.insert(filenameLow);
			}
		}
	}
}